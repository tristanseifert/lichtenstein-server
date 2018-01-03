To properly build this project, you will need to [download the AngelScript SDK](http://www.angelcode.com/angelscript/downloads.html) (as of writing, the latest version is 2.32.0, released 2017-12-16) and into the `angelscript-sdk` folder, and then build both AngelScript and the optional add-ons as static libraries. 

If you get link errors after doing this, you _may_ have to re-compile AngelScript with the `ANGELSCRIPT_EXPORT` flag set.
