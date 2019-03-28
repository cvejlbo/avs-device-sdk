add_definitions("-DACSDK_LOG_MODULE=syntiantKeywordDetector")
add_library(SYNTIANT SHARED
    SyntiantKeywordDetector.cpp)

target_include_directories(SYNTIANT PUBLIC
	"${SYNTIANT_KEY_WORD_DETECTOR_INCLUDE_DIR}"
	"${KWD_SOURCE_DIR}/include" 
	"${SYNTIANT_SOURCE_DIR}/include")

target_link_libraries(SYNTIANT KWD AVSCommon "${SYNTIANT_KEY_WORD_DETECTOR_LIB_PATH}")

# install target
asdk_install()
