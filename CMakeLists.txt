cmake_minimum_required(VERSION 3.7)
project(autodeploy)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        src/BlockingQueue.h
        src/DeployWorker.cpp
        src/DeployWorker.h
        src/EpollPoller.cpp
        src/EpollPoller.h
        src/FileSystemWatcher.cpp
        src/FileSystemWatcher.h
        src/main.cpp
        src/ProcessWatcher.cpp
        src/ProcessWatcher.h
        src/RuntimeError.h
)

add_executable(autodeploy ${SOURCE_FILES})
if (UNIX)
    target_link_libraries (autodeploy pthread)
endif ()