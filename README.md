# termux-am-socket

This is a small program for sending commands to the Termux:API app,
thereby allowing terminal programs to use the Android API. The program
replaces [termuxAm](https://github.com/termux/termuxAm), and uses a
unix socket to transfer information between the app and userspace,
thereby speeding up api commands by almost a factor 10.


## License

Released under the [GPLv3 license](http://www.gnu.org/licenses/gpl-3.0.en.html).
