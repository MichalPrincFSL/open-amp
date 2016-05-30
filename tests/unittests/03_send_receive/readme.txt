- test all send send functions with valid/invalid parameters

NOTE: 'rpmsg_send_offchannel_raw' does not check 'data' parameter for NULL
Is it feature or bug ?? However, the send wrapper functions perform this
check.


