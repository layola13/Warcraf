# Warcraf -- Warcraft III 1.27a engine memory-layout description, plus a
# clean-room reconstruction of the network layer (see README.md).
#
# NOTE: this project describes the memory layout of the 32-bit War3 1.27a
# binaries (Game.dll / Storm.dll / Mss32.dll). Most .cpp files are
# placeholder stubs that only include their header (plus empty
# default-returning bodies for the seven Storm functions Storm.h
# declares). The value of the headers is the layout: verified offsets,
# vtable layouts, and ordinal tables, each with its evidence.
#
# EXCEPTION: the network layer under src/warcraf/net/ (NetChecksum,
# NetDataStore, NetCommandCodec, NetSession, NetEventPump, TurnsSync,
# NetTransport) is REAL, runnable code -- a reconstruction of the original
# behaviour from the Ghidra decompiles (net_decomp.txt / sync_decomp.txt)
# and the S1 analysis. CBattleNet.cpp is still a stub: Battle.net is out
# of scope for the reconstruction. See README.md for the evidence map.
#
# Layout asserts: the headers assert their pinned sizes/offsets at compile
# time. Asserts involving pointers only fire on a 32-bit target (see
# WARCRAF_32BIT_LAYOUT in include/warcraf/core/Types.h); on a 64-bit host
# only the pointer-free subset is checked. For full 32-bit verification
# build with a 32-bit target (MSVC x86, or g++ -m32 with multilib).

cmake_minimum_required(VERSION 3.16)
project(warcraf CXX)

add_library(warcraf STATIC
  # core
  src/warcraf/core/Types.cpp
  src/warcraf/core/Storm.cpp
  src/warcraf/core/CRng.cpp
  # math
  src/warcraf/math/Vector2.cpp
  src/warcraf/math/Vector3.cpp
  # net -- real implementations (reconstruction from the decompiles).
  # The layout headers CNetData.h / NetRouter.h / CNetCommand.h have no
  # .cpp of their own anymore: their old placeholder stubs were replaced
  # by the behavioural implementations below. CBattleNet.cpp is still a
  # stub (Battle.net is out of scope for the reconstruction).
  src/warcraf/net/NetChecksum.cpp
  src/warcraf/net/NetDataStore.cpp
  src/warcraf/net/NetCommandCodec.cpp
  src/warcraf/net/NetSession.cpp
  src/warcraf/net/NetEventPump.cpp
  src/warcraf/net/TurnsSync.cpp
  src/warcraf/net/NetTransport.cpp
  src/warcraf/net/CBattleNet.cpp
  # io
  src/warcraf/io/SFile.cpp
  src/warcraf/io/CReplay.cpp
  # engine
  src/warcraf/engine/CMainLoop.cpp
  # script
  src/warcraf/script/Jass.cpp
  # sim
  src/warcraf/sim/CharArray.cpp
  src/warcraf/sim/StringArray.cpp
  src/warcraf/sim/AbilityUIDef.cpp
  src/warcraf/sim/AbilityLevelData.cpp
  src/warcraf/sim/AbilDataCacheNode.cpp
  src/warcraf/sim/CAgent_VTable.cpp
  src/warcraf/sim/CAgentWar3_VTable.cpp
  src/warcraf/sim/CWar3Image_VTable.cpp
  src/warcraf/sim/CWidget_VTable.cpp
  src/warcraf/sim/CSelectable_VTable.cpp
  src/warcraf/sim/CUnit_VTable.cpp
  src/warcraf/sim/CUnitRefList_VTable.cpp
  src/warcraf/sim/CAbility_VTable.cpp
  src/warcraf/sim/CAgent.cpp
  src/warcraf/sim/CAgentWar3.cpp
  src/warcraf/sim/CWar3Image.cpp
  src/warcraf/sim/CWidget.cpp
  src/warcraf/sim/CSelectable.cpp
  src/warcraf/sim/CUnit.cpp
  src/warcraf/sim/CUnitRefList.cpp
  src/warcraf/sim/CAbility.cpp
  src/warcraf/sim/main.cpp
  # render
  src/warcraf/render/CGx.cpp
  # ui
  src/warcraf/ui/CFrame.cpp
  # audio
  src/warcraf/audio/CMiles.cpp
)

target_include_directories(warcraf PUBLIC include)
target_compile_features(warcraf PUBLIC cxx_std_17)

# Network-layer demo and unit tests (see README.md "Network layer").
# The demo runs two loopback peers exchanging turns over real UDP
# sockets on 127.0.0.1; the tests check the checksum, codec, lockstep
# accounting, pump, turn-packet parse, and loopback transport.
add_executable(warcraf_net_demo apps/net_demo/main.cpp)
target_link_libraries(warcraf_net_demo PRIVATE warcraf)

add_executable(warcraf_net_tests tests/net_tests.cpp)
target_link_libraries(warcraf_net_tests PRIVATE warcraf)


