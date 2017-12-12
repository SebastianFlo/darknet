# Person Detection - alpha.0.0 #
This project uses the Darknet neural network to objects and persons from a camera stream. The detected objects and the probability is then printed out. 

## Installation 

clone this repo
```
git clone https://github.com/SebastianFlo/darknet
```

build 
```
bash install.sh
```

## Running

```
./darknet detector demo -c 1 cfg/voc.data cfg/tiny-yolo-voc.cfg  tiny-yolo-voc.weights
```


Output should be: 

```
FPS:1.0
Objects:

1513079316 : starting here
{ timestamp: 1513079316, payload: [ { "name": "person", "prob": 0.702280 } ]}
```


## Extra Options

If a different camera needs to be used, run the followign command

```
./darknet detector demo -c 1 cfg/voc.data cfg/tiny-yolo-voc.cfg  tiny-yolo-voc.weights

```

Where the important flag `-c 1` points to the index of the camera. Default is `0`, second camera is `1`, etc..