- send user data through default channel
- create several custom epts and send data through them

NOTE: rpmsg_create_channel does *not working* correctly.

- It creates channel on REMOTE side but does not send NS
message to MASTER because 'get_buffer' function does not 
use timeout to wait for available buffer.
- NS function ends with error that is not reported to called fn.
- It requires to have specification/agreement of expected 
  behaviour - otherwise it's not possible make a fix

