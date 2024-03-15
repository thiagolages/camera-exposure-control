#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#ifndef HIDIOCSFEATURE
#warning Please have your distro update the userspace kernel headers
#define HIDIOCSFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x06, len)
#define HIDIOCGFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x07, len)
#endif
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <ctype.h>

#define Vendor_ID 0x2560
#define Product_ID 0xc128 //(for See3CAM_24CUG)

#define CAMERA_CONTROL_24CUG    0xA8

#define GET_EXP_ROI_MODE_24CUG	0x07
#define SET_EXP_ROI_MODE_24CUG	0x08
#define SET_STREAM_MADE_24CUG	0x1C

// General variables/parameters 
int fd;                         // device
uint8_t buf[64],in_buf[64];     // buffers

#define ResolnWidth  1280       // camera default resolution 
#define ResolnHeight 720        // camera default resolution

// debugging verbose
bool debug = true;

// -------------------------------------------------------------------------
// -------------- Don't change anything below this line: -------------------
// -------------------------------------------------------------------------
int i, res, desc_size = 0;
struct hidraw_report_descriptor rpt_desc;
struct hidraw_devinfo info;
char device[16] = "/dev/hidraw";
int Split_number[4] = {0};
uint32_t ROI_mode   = 2; // ROI Auto Exposure Mode: (1 = Full Mode, 2 = Manual Mode)
uint32_t stream_val = 0, exposure_compensation = 0, framerate_ctrl = 0;

//((Input - InputLow) / (InputHigh - InputLow)) * (OutputHigh - OutputLow) + OutputLow // map resolution width and height -  0 to 255
double inputXCord   = 0;
int outputXCord     = 0;
double inputYCord   = 0;
int outputYCord     = 0;

double outputLow    = 0;
double outputHigh   = 255;

double inputXLow    = 0;
double inputXHigh   = ResolnWidth-1;

double inputYLow    = 0;
double inputYHigh   = ResolnHeight-1;

char* cameraModel;

char* supportedCameraModels[] = {"24CUG", NULL}; // make sure there's a NULL pointer at the end, otherwise, we'll have trouble counting the char**;

void checkParameters(int);
void findCamera();
void checkCameraModel(char*);
void lockExposure(bool);
void setROIExposure(int,int,int);
void runCommand(int, char**);
void printSupportedCameraModels();
void printUsageTable();
void setROIMode(int);

// usage: ./cameraExposureControl cameraModel command options
// commands: 
// 0 - lock exposure
//      -> options: 0 for false (unlock), 1 for true (lock)
// 1 - set ROI
//      -> options: xCoordinate yCoordinate (number between 0 and 1, as a porcentage of widht/height)
int main(int argc, char *argv[]){

    checkParameters(argc);          // check if number of parameters is correct
    checkCameraModel(argv[1]);      // check if camera model is supported
    findCamera();                   // find HID device
    runCommand(argc, argv);      // run desired command
    
    return 0;
}

int numberOfStrings(char** array){
    int numStrings=0;
    char** arrPtr = array;
    
    while (*arrPtr != NULL){
        numStrings++;
        arrPtr++;
    }
    return numStrings;
}

void printSupportedCameraModels(){
    printf("Currently supported camera models: \n");
    int i=0;
    int sizeSupportedCameraModels = numberOfStrings(supportedCameraModels);
    for (i=0; i < sizeSupportedCameraModels; i++){
        printf("%d) %s ",i+1, supportedCameraModels[i]);
    }
    printf("\n");
}

void printUsageTable(){

    // Print the divider row
    printf("|-----------------|-----------------|--------------------------------|\n");

    // Print the content rows
    for (int i = 0; i < 4; i++) {
        if (i == 0) {
            // Row 1
            printf("| %-15s | %-12s | %-22s |\n", "", "\tCommand", "\t\tOptions");
        } else if (i == 1) {
            // Row 2
            printf("| %-15s | %-12s | %-29s |\n", "Lock", "\t0", "\t0(Unlock) or 1(Lock)");
        } else if (i == 2) {
            // Row 3
            printf("| %-15s | %-12s | %-29s |\n", "Set ROI (x,y)", "\t1", "\tX,Y coordinates");
        }
        else if (i == 3) {
            // Row 3
            printf("| %-15s | %-12s | %-29s |\n", "Set ROI Mode", "\t2", "\t1(Full-Auto) or 2(Manual)");
        }

        // Print the divider row after each content row
        printf("|-----------------|-----------------|--------------------------------|\n");
    }

}

void checkParameters(int argc){
    if (argc < 4){
        printUsageTable();
        printf("Usage: ./cameraExposureControl cameraModel command options\n");
        printSupportedCameraModels();
        exit(-1);
    }
}

void runCommand(int argc, char** argv){
    int command = atoi(argv[2]);
    switch (command){
        case 0: ;// "lock"
            bool lock = (bool)atoi(argv[3]);
            lockExposure(lock);
            break;
        case 1: // "roi"
            if (argc < 5){ // need X and Y coordinate, so 5 arguments in total
                printf("Need X and Y coordinates.\n");
                exit(-1);
            } 
            float xCoordFloat = atof(argv[3]);
            float yCoordFloat = atof(argv[4]);

            if (xCoordFloat < 0 || xCoordFloat > 1.0 || yCoordFloat < 0.0 || yCoordFloat > 1.0){
                printf("xCoord and yCoord must be between 0.0 and 1.0\n");
                exit(-1);
            }
            int xCoord = (int)(xCoordFloat*ResolnWidth);
            int yCoord = (int)(yCoordFloat*ResolnHeight);
            int winSize = 1;           
            
            lockExposure(false);
            // printf("Setting ROI exposure for x=%f, y=%f.\n",xCoordFloat, yCoordFloat);
            setROIExposure(xCoord, yCoord, winSize);
            lockExposure(true);
            break;

        case 2: // set ROI mode to 1(Full-Auto) or 2(Manual)
            
            if (argc < 4){ // need ROI mode, so 4 arguments in total
                printf("Need ROI Mode: 1-Full(Auto), 2-Manual.\n");
                exit(-1);
            } 
            
            setROIMode(atoi(argv[3]));
            
            break;

        case 3: // get ROI mode (not used)
            buf[0] = CAMERA_CONTROL_24CUG;
            buf[1] = GET_EXP_ROI_MODE_24CUG;
            res = write(fd,buf,64);
            res = 0;
            while(res != 64){	
                res = read(fd,in_buf,64);
                // if things go wrong
                if(in_buf[0] != CAMERA_CONTROL_24CUG ||
                    in_buf[1]!=SET_STREAM_MADE_24CUG  ||
                    in_buf[6]!=0x01)
                    res = 1;
            }
            printf("Current get ROI Auto Exspousre Mode: X Co-Ordinates = %d,Y Co-Ordinates = %d,Window_size = %d, pos[2] = %d\n", in_buf[3],in_buf[4],in_buf[5], in_buf[2]);         

        default:
            printf("Wrong option. \t Enter correct option. \n");
            break;
    }
}

void setROIMode(int ROIMode){

    if (ROIMode != 1 && ROIMode != 2){
        printf("ROI Mode has to be either 1(Full-Auto) or 2(Manual).\n");
        exit(-1);
    }

    if (debug)
        printf("Setting ROI Mode to %s.\n",(ROIMode == 1 ? "Full (Auto)" : "Manual"));

    // fill buffer values
    buf[0] = CAMERA_CONTROL_24CUG  ;    /* set camera control code */
    buf[1] = SET_EXP_ROI_MODE_24CUG ;   /* set stream mode command code */
    buf[2] = ROIMode;                   /* ROI Mode */

    res = write(fd,buf,64);
    res = 0;

    while(res != 64){	
        res = read(fd,in_buf,64);
        if(buf[1] != SET_EXP_ROI_MODE_24CUG)
            res = 1;
    }
}

void setROIExposure(int xCoord, int yCoord, int winSize){

    if (debug)
        printf("Setting ROI exposure for x=%d, y=%d.\n",xCoord, yCoord);

    // mapping to 0-255
    inputXCord = xCoord;
    outputXCord = ((inputXCord - inputXLow) / (inputXHigh - inputXLow)) * (outputHigh - outputLow) + outputLow;
    
    inputYCord = yCoord;
    outputYCord = ((inputYCord - inputYLow) / (inputYHigh - inputYLow)) * (outputHigh - outputLow) + outputLow;

    // fill buffer values
    buf[0] = CAMERA_CONTROL_24CUG  ;    /* set camera control code */
    buf[1] = SET_EXP_ROI_MODE_24CUG ;   /* set stream mode command code */
    buf[2] = 0x02;                      /* ROI Mode has to be 2 - Manual Mode */

    buf[3] = outputXCord; // x cord
    buf[4] = outputYCord; // y cord
    buf[5] = winSize; // window size

    res = write(fd,buf,64);
    res = 0;
    while(res != 64){	
        res = read(fd,in_buf,64);
        if(buf[1] != SET_EXP_ROI_MODE_24CUG)
            res = 1;
    }
}

void lockExposure(bool lock){
    
    if (debug)
        printf("%sing exposure.\n",lock?"Lock":"Unlock");

    // fill buffer values
    buf[0] = CAMERA_CONTROL_24CUG;      /* set camera control code */
    buf[1] = SET_STREAM_MADE_24CUG ;    /* set stream mode code */
    buf[2] = 0x00;                      /* actual stream mode */
    buf[3] = lock;
    
    res = write(fd,buf,64);
    res = 0;
    
    while(res != 64){
        res = read(fd,in_buf,64);
        if(buf[1] != SET_STREAM_MADE_24CUG)
            res = 1;
    }
}

void checkCameraModel(char* cameraModel){
    // transform to lowercase
    for(int i = 0; cameraModel[i]; i++){
        cameraModel[i] = tolower(cameraModel[i]);
    }

    if (debug)
        printf("Camera Model = %s\n", cameraModel);
    
    // check if camera is supported
    if ( strcmp( cameraModel, "24cug") != 0){
        printf("24CUG is the only currently supported model.\n");
        exit(-1);
    }
}

void findCamera(){
    int number=0, temp, digit_count, itr,camera_found=0;
    while(number<200){	
		temp = number;
		if(temp == 0){
			device[11] = number + '0';
			device[12] = '\0';
		}
		else{
			digit_count = 0;
			itr=11;
			while(temp >0){
				Split_number[digit_count++] = temp%10;
				temp/=10;
			}
			for(i=digit_count-1;i>=0;i--){
				device[itr++] = Split_number[i] + '0';
			}
			device[itr] ='\0'; 
		}
		fd = open(device, O_RDWR|O_NONBLOCK);

		if (fd < 0) {
			number++;
			continue;
		}

		memset(&rpt_desc, 0x0, sizeof(rpt_desc));
		memset(&info, 0x0, sizeof(info));
		memset(buf, 0x0, sizeof(buf));
		res = ioctl(fd, HIDIOCGRDESCSIZE, &desc_size);
		if (res < 0)
			perror("HIDIOCGRDESCSIZE");
		rpt_desc.size = desc_size;
		res = ioctl(fd, HIDIOCGRDESC, &rpt_desc);
		if (res < 0) {
			perror("HIDIOCGRDESC");
		}
		res = ioctl(fd, HIDIOCGRAWPHYS(256), buf);
		if (res < 0)
			perror("HIDIOCGRAWPHYS");
		res = ioctl(fd, HIDIOCGRAWINFO, &info);
		if (res < 0) {
			perror("HIDIOCGRAWINFO");
		}
		if(info.vendor == Vendor_ID && (info.product & 0xFFFF) == Product_ID){
			camera_found = 1;
			res = ioctl(fd, HIDIOCGRAWNAME(256), buf);
			if (res < 0)
				perror("HIDIOCGRAWNAME");
			// else
			// 	printf("\n\rConnected Device: %s", buf);
			break;
		}
		number++;
		close(fd);
	}
	if(camera_found == 0){
		printf("\n\rSee3CAM_24CUG cannot be found");
		exit(-1);
	}
}
