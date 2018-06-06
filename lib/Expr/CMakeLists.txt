#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
find_package(protobuf)
find_package(capnp)
klee_add_component(kleaverExpr
  ArrayCache.cpp
  Assigment.cpp
  Constraints.cpp
  ExprBuilder.cpp
  Expr.cpp
  ExprEvaluator.cpp
  ExprPPrinter.cpp
  ExprSMTLIBPrinter.cpp
  ExprUtil.cpp
  ExprVisitor.cpp
  Lexer.cpp
  Parser.cpp
  Updates.cpp
)

set(LLVM_COMPONENTS
  support
)

klee_get_llvm_libs(LLVM_LIBS ${LLVM_COMPONENTS})
if(PROTOBUF_FOUND)
    klee_add_component(protoExpr Cache.pb.cc)
    target_link_libraries(kleaverExpr PUBLIC protoExpr protobuf)
endif()
if(CAPNP_FOUND)
    klee_add_component(capExpr cache_cap.capnp.cpp)
    target_link_libraries(kleaverExpr PUBLIC capnp kj capExpr)
endif()
target_link_libraries(kleaverExpr PUBLIC ${LLVM_LIBS})
