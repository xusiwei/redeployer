cmake_minimum_required(VERSION 2.8)
project(autodeploy)

set(CMAKE_CXX_STANDARD 11)

set(COMMON_SOURCE_FILES
        src/BlockingQueue.h
        src/DeployWorker.cpp
        src/DeployWorker.h
        src/EpollPoller.cpp
        src/EpollPoller.h
        src/FileSystemWatcher.cpp
        src/FileSystemWatcher.h
        src/ProcessWatcher.cpp
        src/ProcessWatcher.h
        src/RuntimeError.h
        src/FunctionScheduler.cpp
        src/FunctionScheduler.h)

add_executable(autodeploy ${COMMON_SOURCE_FILES} src/main.cpp)
if (UNIX)
    target_link_libraries (autodeploy pthread)
endif ()

add_executable(fs_test ${COMMON_SOURCE_FILES} src/fs_test.cpp)
if (UNIX)
    target_link_libraries (fs_test pthread)
endif ()