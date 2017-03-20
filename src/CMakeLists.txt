# 增加opencv的依赖
FIND_PACKAGE( OpenCV REQUIRED )

# 增加一个库文件
ADD_LIBRARY( getConfig getConfig.cpp )
TARGET_LINK_LIBRARIES( getConfig  ${OpenCV_LIBS} )

# 增加一个可执行的二进制sudoku_recgnize,并链接对应的库
ADD_EXECUTABLE( sudoku_recgnize main.cpp )
TARGET_LINK_LIBRARIES( sudoku_recgnize getConfig ${OpenCV_LIBS} )
