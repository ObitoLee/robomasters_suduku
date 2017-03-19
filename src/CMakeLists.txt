# 增加opencv的依赖
FIND_PACKAGE( OpenCV REQUIRED )

# 增加一个库文件
ADD_LIBRARY( getConfig getConfig.cpp )
TARGET_LINK_LIBRARIES( getConfig  ${OpenCV_LIBS} )

# 增加一个可执行的二进制
ADD_EXECUTABLE( main main.cpp )
TARGET_LINK_LIBRARIES( main getConfig ${OpenCV_LIBS} )
