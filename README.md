# camera-exposure-control

Exposure Control of e-con Systems' 24CUG camera model. Can be used for other models

# Commands

```json
|                 | 	Command     | 		Options                  |
|:---------------:|:---------------:|:------------------------------:|
| Exposure Control| 	0           | 	0(Unlock) or 1(Lock)         |
| Set ROI (x,y)   | 	1           | 	X,Y coordinates              |
| Set ROI Mode    | 	2           | 	1(Full-Auto) or 2(Manual)    |
```

```json
exposureCommandMap = {
    "exposureControl"   : 0,
    "roi"               : 1,
    "roiMode"           : 2
},
roiModeOptionMap = {
    "auto"      : 1,
    "full"      : 1, // same as 'auto'
    "manual"    : 2
}
```

# Example usage

## 0) Unlock exposure control
 
 - Unlock 24CUG's ROI change, using command 0 ('exposureControl'), option 0 ('unlock'). Needed before making exposure changes.

```sudo ./cameraExposureControl 24cug 0 0```

## 1) Lock exposure control
 
 - Lock 24CUG's ROI change, using command 0 ('exposureControl'), option 1 ('lock'). Needed to make exposure changes 'persistent'.

```sudo ./cameraExposureControl 24cug 0 1```

## 2) Set exposure based on ROI

- First, set exposure to AUTO, if not yet, otherwise it won't change:

```v4l2-ctl -d /dev/video0 --set-ctrl=exposure_auto=0```

- Second, unlock 24CUG's exposure control, using command 0 ('exposureControl'), option 0 ('auto'/'unlock')

```sudo ./cameraExposureControl 24cug 0 0```

- Third, set desired point in the image (mapped between 0.0-1.0) to be used as ROI (with default params for ROI size), using command 1 ('roi'), and (x,y) coordinates between 0.0-1.0 (float)

| Position          | Command                                           |
|:-----------------:|:-------------------------------------------------:|
|Center of image    |```sudo ./cameraExposureControl 24cug 1 0.5 0.5``` |
|Top left           |```sudo ./cameraExposureControl 24cug 1 0.0 0.0``` |
|Top right          |```sudo ./cameraExposureControl 24cug 1 1.0 0.0``` |
|Bottom left        |```sudo ./cameraExposureControl 24cug 1 0.0 1.0``` |
|Bottom right       |```sudo ./cameraExposureControl 24cug 1 1.0 1.0``` |

## 3) Set auto exposure

- First, set exposure to AUTO, using `v4l2-ctl`

```v4l2-ctl -d /dev/video0 --set-ctrl=exposure_auto=0```

- Second, unlock 24CUG's exposure control, using command 0 ('exposureControl'), option 0 ('auto'/'unlock')

```sudo ./cameraExposureControl 24cug 0 0```

- Third, set ROI mode to Full (and not Manual anymore), using command 2 ('roiMode') option 1 ('full')

```sudo ./cameraExposureControl 24cug 2 1 ```

## 4) Set manual exposure

- First, set exposure to MANUAL, using `v4l2-ctl`

```v4l2-ctl -d /dev/video0 --set-ctrl=exposure_auto=1```

- Second, unlock 24CUG's exposure control, using command 0 ('exposureControl'), option 0 ('auto'/'unlock')

```sudo ./cameraExposureControl 24cug 0 0```

- Third, set ROI mode to Full (and not Manual anymore), using command 2 ('roiMode') option 1 ('full')

```sudo ./cameraExposureControl 24cug 2 1 ```