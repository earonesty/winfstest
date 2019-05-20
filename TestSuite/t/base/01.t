#!/usr/bin/python

# CreateFile FlagsAndAttributes
# DeleteFile

from winfstest import *

name = uniqname()

expect("CreateFile %s GENERIC_WRITE 0 0 CREATE_ALWAYS FILE_ATTRIBUTE_NORMAL 0" % name, 0)

#0x2000 = FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, 0x20 = FILE_ATTRIBUTE_ARCHIVE (0x80 = NORMAL)
expect("GetFileInformation %s" % name, 
        ( lambda r: r[0]["FileAttributes"]&~0x2000 in (0x80, 0x20) ), 
        ( lambda r: r[0]["FileAttributes"] ) 
      )

# expect("CreateFile %s GENERIC_WRITE 0 0 CREATE_ALWAYS FILE_ATTRIBUTE_READONLY 0" % name, 0)

# expect("GetFileInformation %s" % name, 
#        lambda r: r[0]["FileAttributes"]&~0x2000 in (0x21, 0x01),
#        lambda r: r[0]["FileAttributes"] 
#      )

expect("SetFileAttributes %s FILE_ATTRIBUTE_NORMAL" % name, 0)
expect("CreateFile %s GENERIC_WRITE 0 0 CREATE_ALWAYS FILE_ATTRIBUTE_SYSTEM 0" % name, 0)
expect("GetFileInformation %s" % name, lambda r: r[0]["FileAttributes"] in (0x24, 0x04))
expect("SetFileAttributes %s FILE_ATTRIBUTE_NORMAL" % name, 0)
expect("CreateFile %s GENERIC_WRITE 0 0 CREATE_ALWAYS FILE_ATTRIBUTE_HIDDEN 0" % name, 0)
expect("GetFileInformation %s" % name, lambda r: r[0]["FileAttributes"] in (0x22, 0x02))
expect("DeleteFile %s" % name, 0)

# expect("CreateFile %s GENERIC_WRITE 0 0 CREATE_ALWAYS FILE_ATTRIBUTE_READONLY 0" % name, 0)
# expect("DeleteFile %s" % name, "ERROR_ACCESS_DENIED")
# expect("SetFileAttributes %s FILE_ATTRIBUTE_NORMAL" % name, 0)
# expect("DeleteFile %s" % name, 0)

testdone()
