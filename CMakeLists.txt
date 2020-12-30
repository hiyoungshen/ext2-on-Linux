# CMakeLists.txt
# CMake最低版本要求
cmake_minimum_required(VERSION 3.5)

# 项目名称
project(vfs_ext2-110530-linux)

message("\n #### ext building! #### \n")

set(src "./src/")
set(include "./src/")

# set(CMAKE_CXX_STANDARD 11)
# 设置库文件输出到代码源路径下的lib文件夹中
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# REQUIRED表示一定要找到这个库，找不到这个库，编译就直接结束，不再往下进行了
# find_package(OpenCV REQUIRED)

# 指明头文件所在的目录，如果前面使用了find_package(x)去查找到了某一个库，那么就可以使用这个命令去包含这个库的头文件，例如：include_directories(${x_INCLUDE_DIRS}
include_directories(
        ${PROJECT_SOURCE_DIR}/include/
)

add_library(${PROJECT_NAME} SHARED
        src/buf.c
        src/DArray.c
        src/fs.c
        src/inode.c
        src/ReadFile.c
        src/test.c
)

# 生成可执行文件，ext2是可执行文件的名字，hello.c是源文件名称，如有其他源文件，可在后面添加
add_executable(ext2.o src/buf.c src/DArray.c src/fs.c src/inode.c src/ReadFile.c src/test.c) 

# target_link_libraries(i_love_China
#         ${OpenCV_LIBS}
#         ${PROJECT_NAME}
# )
