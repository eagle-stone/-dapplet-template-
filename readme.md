# EAST: An Efficient and Accurate Scene Text Detector

### Introduction
This is a tensorflow re-implementation of [EAST: An Efficient and Accurate Scene Text Detector](https://arxiv.org/abs/1704.03155v2).
The features are summarized blow:
+ Online demo
	+ http://east.zxytim.com/
	+ Result example: http://east.zxytim.com/?r=48e5020a-7b7f-11e7-b776-f23c91e0703e
	+ CAVEAT: There's only one cpu core on the demo server. Simultaneous access will degrade response time.
+ Only **RBOX** part is implemented.
+ A fast Locality-Aware NMS in C++ provided by the paper's author.
+ The pre-trained model provided achieves **80.83** F1-score on ICDAR 2015
	Incidental Scene Text Detection Challenge using only training images from ICDAR 2015 and 2013.
  see [here](http://rrc.cvc.uab.es/?ch=4&com=evaluation&view=method_samples&task=1&m=29855&gtv=1) for the detailed results.
+ Differences from original paper
	+ Use ResNet-50 rather than PVANET
	+ Use dice loss (optimize IoU of segmentation) rather than balanced cross entropy
	+ Use linear learning rate decay rather than staged learning rate decay
+ Speed on 720p (resolution of 1280x720) images:
	+ Now
		+ Graphic card: GTX 1080 Ti
		+ Network fprop: **~50 ms**
		+ NMS (C++): **~6ms**
		+ Overall: **~16 fps**
	+ Then
		+ Graphic card: K40
		+ Netw