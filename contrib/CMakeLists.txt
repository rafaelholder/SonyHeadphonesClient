set(FETCHCONTENT_QUIET FALSE)
include(FetchContent)
find_package(Git REQUIRED)

# Here's a daily reminder that `_SOURCE_DIR` is always in lower case...
if (MDR_MSVC_CLIENT_YY_THUNKS)
    FetchContent_Declare(
            yy_thunks
            URL https://github.com/Chuyu-Team/YY-Thunks/releases/download/v1.2.3/YY-Thunks-Objs.zip
            URL_HASH SHA256=4dc68ec8784e0afed7284e4393aec672873608161c3a5ae52faf618875d95b81
    )
endif ()

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        # Pinned to the commit of tag 12.1.0 (tags are mutable; a SHA is not)
        GIT_TAG 407c905e45ad75fc29bf0f9bb7c5c2fd3475976f
)

if (MDR_PLATFORM_OS STREQUAL "OHOS")
    FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL
            GIT_TAG a24429c192cfee5ae92697f12f219cad27635fb4
    )
else ()
    FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL
            # Pinned to the commit of tag release-3.2.26
            GIT_TAG badbf8da4ee72b3ef599c721ffc9899e8d7c8d90
            PATCH_COMMAND
                "${GIT_EXECUTABLE}" reset --hard
                COMMAND "${GIT_EXECUTABLE}" apply "${CMAKE_CURRENT_SOURCE_DIR}/SDL-disable-dynapi.patch"
    )
endif ()

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        # Pinned to the commit of tag v1.92.7
        GIT_TAG dac07199cfd761113d966eb8ad739254e10df2fe
)

