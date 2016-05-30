- send user data through default channel
- create several custom epts and send data through them

NOTE: rpmsg_rtos layer does not support creating additional channels (bug/feature ??) !

Termination rpmsg channel on REMOTE without sending agreement inside application code
*might* cause an error on MASTER. Fix requires significant improvement of RTOS layer and
further testing.

