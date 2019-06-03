# Cisco Project Lava
School sponsored research project for optimized packet passing for virtual networks

## Introduction

Virtualization is the process of using software to emulate what is normally accomplished with hardware, allowing better utilization of physical resources. Cisco has been moving towards virtualization in order to improve network efficiency and reduce the total cost of operation. However, Cisco discovered that passing packets between two sets of threads (workloads) within a virtual system is a bottleneck. The purpose of this project is to research various algorithms and optimization techniques that might reduce this bottleneck, increasing throughput between workloads. The aim is to surpass the industry hardware standard of 10Gbs, which is about 14 million packets per second. 

## Prerequisites
Run on a 20 core system  
For systems with less than 20 cores:  
- modify the macros in global.h:  
    - INPUT_BASE_CORE  
    - OUTPUT_BASE_CORE  
- Modify the functions in framework.c:  
    - spawn_input_threads():207  
    - spawn_output_threads():230  

## Installation
Clone the repo:  
```
git clone https://github.com/codyscode/project-lava.git
```  
Clone the wiki into the same directory as the repo:  
```
git clone https://github.com/codyscode/project-lava.wiki.git
```  

## How to run
Refer to readme under TestingEnvironment directory.   

## Known Bugs
None so far

## Authors
Reginald Chand - [rzchand](https://github.com/rzchand)  
Cody Cunningham - [codyscode](https://github.com/codyscode)  
Lito Pineda - [Repineda](https://github.com/repineda)  
Anand Prabhakar - [anprabha](https://github.com/anprabha)  
Ronuel Benjamin Tan - [RonuelTan](https://github.com/RonuelTan)  
Alex Widmann - [ALKW](https://github.com/alkw)  


## Acknowledgments  
We would like to thank Ian Wells and Kyle Mestery, distinguished engineers at Cisco, for all of their work and involvement in this project. As well as professors Richard Jullig, Patrick Mantey, and Morteza Behrooz for all of their time and assistance.  


