# CMake generated Testfile for 
# Source directory: /saturn/noiz2sa-0.51a-saturn
# Build directory: /saturn/noiz2sa-0.51a-saturn
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(sh2_collision_campaign "bash" "/saturn/noiz2sa-0.51a-saturn/Tests/test_campaign.sh" "--emulator" "mednafen" "--strict")
set_tests_properties(sh2_collision_campaign PROPERTIES  TIMEOUT "700" _BACKTRACE_TRIPLES "/saturn/noiz2sa-0.51a-saturn/CMakeLists.txt;1165;add_test;/saturn/noiz2sa-0.51a-saturn/CMakeLists.txt;0;")
add_test(sh2_background_animation_campaign "bash" "/saturn/noiz2sa-0.51a-saturn/Tests/test_background_campaign.sh" "--emulator" "mednafen" "--strict")
set_tests_properties(sh2_background_animation_campaign PROPERTIES  TIMEOUT "700" _BACKTRACE_TRIPLES "/saturn/noiz2sa-0.51a-saturn/CMakeLists.txt;1171;add_test;/saturn/noiz2sa-0.51a-saturn/CMakeLists.txt;0;")
add_test(bulletml_parity_test "/saturn/noiz2sa-0.51a-saturn/Tests/bulletml_parity_ut")
set_tests_properties(bulletml_parity_test PROPERTIES  TIMEOUT "60" _BACKTRACE_TRIPLES "/saturn/noiz2sa-0.51a-saturn/CMakeLists.txt;1177;add_test;/saturn/noiz2sa-0.51a-saturn/CMakeLists.txt;0;")
add_test(bulletml_all_xml_recursive_test "python3" "/saturn/noiz2sa-0.51a-saturn/Tests/bulletml/test_all_xml_recursive.py")
set_tests_properties(bulletml_all_xml_recursive_test PROPERTIES  TIMEOUT "120" _BACKTRACE_TRIPLES "/saturn/noiz2sa-0.51a-saturn/CMakeLists.txt;1183;add_test;/saturn/noiz2sa-0.51a-saturn/CMakeLists.txt;0;")
subdirs("src/bulletml_binary")
