# MomentShadowMapping
Continuing from my deferred renderer project, I expanded framework to include support for filterable shadow maps using the [Moment Shadow Mapping](https://cg.cs.uni-bonn.de/en/publications/paper-details/peters-2015-msm/) method.
I was already familiar with the technique behind the Variance Shadow Maps by experimenting with them many years ago, so I was excited to see how this novel MSM technique provides
"substantially higher quality" shadows, as the paper claims.  Needless to say, I was somewhat disappointed but still glad that I got the opportunity to try it out for myself.

## Main Features:
*  Deferred shading with Real-Time soft shadows utilizing the Moment Shadow Map technique.
*  Ability to change the shadow filtering kernel size dynamically.  Supports 7x7, 15x15, 23x23, 35x25, and higher shadow filtering using "moving averages" box filter done in a compute shader.
*  Run-time debuggin of shadow map and light configuration thru the utilization of Dear ImGui library

## Things I Learned:
*  Using the Hamburger 4MSM algorithm indeed produces very nice soft shadows.
*  As the paper suggested, I first tried using the biad value (0.00003) and didn't notice any obvious light-bleeding artifacts.  I then moved the light source around and quickly ran into what the paper described as "slight quantization noise."
Those artifacts were quite noticeable. Increasing the bias value by a factor of 10 did eliminate this ugly quantization noise completely, but then the light-bleeding will become more
obvious.  So, basically, just like their predecessors (VSM and EVSM), the Moment Shadows suffer from similar precision issues and require having just the right biasing value to 
produce acceptable results.  Not to mention doing more per pixel calculations and even utilizing up to 128 bits per texel, which is quite a bit of memory and bandwidth.

![Alt Text](https://github.com/timurson/MomentShadowMapping/blob/master/Image1.PNG)
![Alt Text](https://github.com/timurson/MomentShadowMapping/blob/master/Image2.PNG)
![Alt Text](https://github.com/timurson/MomentShadowMapping/blob/master/Image3.PNG)



# License
Copyright (C) 2021 Roman Timurson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
