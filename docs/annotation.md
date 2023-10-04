
# Annotation library

The *Annotation* library provides graphical tools to:

-	Manually annotate infrared videos,
-	Export annotations to JSON or to a MySQL/SQLite database,
-	Read back annotations from a JSON file or a MySQL/SQLite database.

The format of video annotations (both JSON and SQL database) is described in the [thermal-events](https://github.com/IRFM/thermal-events) library.

To benefit all features available within the *Annotation* library, several prerequisites must be fullfilled:

-	Qt must be built with the mysql plugin
-	The [librir](https://github.com/IRFM/librir) library must be built and next to the Thermavip executable.
	Use the cmake build option `-DWITH_LIBRIR=ON` for that.
-	You should build an annotation database following instructions from the [thermal-events](https://github.com/IRFM/thermal-events) library.
	Note that Thermavip will use the same .env file as thermal-events, which must be located next to Thermavip executable.
	
