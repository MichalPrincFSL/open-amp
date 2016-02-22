# Change Log
All notable changes to this project will be documented in this file.

## [1.0.0] - 2015-11-19
### :heavy_plus_sign: Added
+ **Generic RTOS aware API**
  + Added [rpmsg_rtos.c] and [rpmsg_rtos.h] files implementing the  RTOS aware layer.
  + Blocking, thread-safe API.
+ **Nocopy messaging mechanism**
  + Added [rpmsg_ext.c] and [rpmsg_ext.h] files implementing the  nocopy send and receive.
  + New nocopy API available in both baremetal and RTOS API.
+ **New Platforms**
  + Added support for i.MX6 platform in [porting/imx6sx_m4/platform.c] 
    and [porting/imx6sx_m4/platform.h].
+ **FreeRTOS support for RTOS aware API**
  + FreeRTOS porting layer implemented for generic RTOS aware API.
  + Added [porting/env/freertos/rpmsg_porting.c] and [porting/env/freertos/rpmsg_porting.h].
  
### :recycle: Changed
- **Separation of _environment_ and _platform_**
  - Environment (baremetal vs. freertos vs. :grey_question:) and platform (zynq vs. i.MX6 vs.a :grey_question:)
    are now separated.
  - Old bm_env.c moved to [porting/env/bm/rpmsg_porting.c], added associated header file.

### :exclamation: Fixed
* Fixed ```rpmsg_get_address()``` function in [rpmsg_core.c] to return a unique address. 
* Fixed mutex resource leak in ```rpmsg_send_ns_message()``` function in [rpmsg_core.c] (return without mutex unlock).
* Fixed rpmsg internal buffer leak under certain circumstances in ```rpmsg_send_offchannel_raw()``` function.

[rpmsg_rtos.c]:rpmsg/rpmsg_rtos.c
[rpmsg_rtos.h]:rpmsg/rpmsg_rtos.h
[rpmsg_ext.c]:rpmsg/rpmsg_ext.c
[rpmsg_ext.h]:rpmsg/rpmsg_ext.h
[porting/imx6sx_m4/platform.c]:porting/imx6sx_m4/platform.c
[porting/imx6sx_m4/platform.h]:porting/imx6sx_m4/platform.h
[porting/env/freertos/rpmsg_porting.c]:porting/env/freertos/rpmsg_porting.c
[porting/env/freertos/rpmsg_porting.h]:porting/env/freertos/rpmsg_porting.h
[porting/env/bm/rpmsg_porting.c]:porting/env/bm/rpmsg_porting.c
[rpmsg_core.c]:rpmsg/rpmsg_core.c
