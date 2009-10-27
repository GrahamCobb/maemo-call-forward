#
# Regular cron jobs for the call-forward package
#
0 4	* * *	root	[ -x /usr/bin/call-forward_maintenance ] && /usr/bin/call-forward_maintenance
