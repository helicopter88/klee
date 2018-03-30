//===-- Expr.cpp ----------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Expr.h"
#include "klee/Config/Version.h"

#include "llvm/ADT/Hashing.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
// FIXME: We shouldn't need this once fast constant support moves into
// Core. If we need to do arithmetic, we probably want to use APInt.
#include "klee/Internal/Support/IntEvaluation.h"

#include "klee/util/ExprPPrinter.h"

#include <sstream>
#include <klee/util/Cache.pb.h>
#include <klee/util/ArrayCache.h>
#include <klee/util/cache_cap.capnp.h>
#include <capnp/message.h>
#include <capnp/serialize-packed.h>

using namespace klee;
using namespace llvm;

namespace {
  cl::opt<bool>
  ConstArrayOpt("const-array-opt",
	 cl::init(false),
	 cl::desc("Enable various optimizations involving all-constant arrays."));
}

/***/

unsigned Expr::count = 0;

ref<Expr> Expr::createTempRead(const Array *array, Expr::Width w) {
  UpdateList ul(array, 0);

  switch (w) {
  default: assert(0 && "invalid width");
  case Expr::Bool:
    return ZExtExpr::create(ReadExpr::create(ul,
                                             ConstantExpr::alloc(0, Expr::Int32)),
                            Expr::Bool);
  case Expr::Int8:
    return ReadExpr::create(ul,
                            ConstantExpr::alloc(0,Expr::Int32));
  case Expr::Int16:
    return ConcatExpr::create(ReadExpr::create(ul,
                                               ConstantExpr::alloc(1,Expr::Int32)),
                              ReadExpr::create(ul,
                                               ConstantExpr::alloc(0,Expr::Int32)));
  case Expr::Int32:
    return ConcatExpr::create4(ReadExpr::create(ul,
                                                ConstantExpr::alloc(3,Expr::Int32)),
                               ReadExpr::create(ul,
                                                ConstantExpr::alloc(2,Expr::Int32)),
                               ReadExpr::create(ul,
                                                ConstantExpr::alloc(1,Expr::Int32)),
                               ReadExpr::create(ul,
                                                ConstantExpr::alloc(0,Expr::Int32)));
  case Expr::Int64:
    return ConcatExpr::create8(ReadExpr::create(ul,
                                                ConstantExpr::alloc(7,Expr::Int32)),
                               ReadExpr::create(ul,
                                                ConstantExpr::alloc(6,Expr::Int32)),
                               ReadExpr::create(ul,
                                                ConstantExpr::alloc(5,Expr::Int32)),
                               ReadExpr::create(ul,
                                                ConstantExpr::alloc(4,Expr::Int32)),
                               ReadExpr::create(ul,
                                                ConstantExpr::alloc(3,Expr::Int32)),
                               ReadExpr::create(ul,
                                                ConstantExpr::alloc(2,Expr::Int32)),
                               ReadExpr::create(ul,
                                                ConstantExpr::alloc(1,Expr::Int32)),
                               ReadExpr::create(ul,
                                                ConstantExpr::alloc(0,Expr::Int32)));
  }
}

ProtoExpr* Expr::serialize() const {
    auto* pe = new ProtoExpr;
    pe->set_width(this->getWidth());
    pe->set_kind(this->getKind());
    for(unsigned k = 0; k < this->getNumKids(); k++) {
        auto pKid = this->getKid(k);
        pe->mutable_kids()->AddAllocated(pKid->serialize());
    }
    return pe;
}

void Expr::serialize(CacheExpr::Builder&& builder) const {
    builder.setKind(this->getKind());
    builder.setWidth(this->getWidth());
    auto kids = builder.initKids(this->getNumKids());
    unsigned i = 0;
    for(; i < this->getNumKids(); i++) {
        this->getKid(i)->serialize(kids[i]);
    }

}
int Expr::compare(const Expr &b) const {
  static ExprEquivSet equivs;
  int r = compare(b, equivs);
  equivs.clear();
  return r;
}

// returns 0 if b is structurally equal to *this
int Expr::compare(const Expr &b, ExprEquivSet &equivs) const {
  if (this == &b) return 0;

  const Expr *ap, *bp;
  if (this < &b) {
    ap = this; bp = &b;
  } else {
    ap = &b; bp = this;
  }

  if (equivs.count(std::make_pair(ap, bp)))
    return 0;

  Kind ak = getKind(), bk = b.getKind();
  if (ak!=bk) {
      return (ak < bk) ? -1 : 1;
  }
  if (hashValue != b.hashValue) {
      return (hashValue < b.hashValue) ? -1 : 1;
  }
  if (int res = compareContents(b)) {
      return res;
  }

  unsigned aN = getNumKids();
  for (unsigned i=0; i<aN; i++)
    if (int res = getKid(i)->compare(*b.getKid(i), equivs)) {
        return res;
    }

  equivs.insert(std::make_pair(ap, bp));
  return 0;
}

void Expr::printKind(llvm::raw_ostream &os, Kind k) {
  switch(k) {
#define X(C) case C: os << #C; break
    X(Constant);
    X(NotOptimized);
    X(Read);
    X(Select);
    X(Concat);
    X(Extract);
    X(ZExt);
    X(SExt);
    X(Add);
    X(Sub);
    X(Mul);
    X(UDiv);
    X(SDiv);
    X(URem);
    X(SRem);
    X(Not);
    X(And);
    X(Or);
    X(Xor);
    X(Shl);
    X(LShr);
    X(AShr);
    X(Eq);
    X(Ne);
    X(Ult);
    X(Ule);
    X(Ugt);
    X(Uge);
    X(Slt);
    X(Sle);
    X(Sgt);
    X(Sge);
#undef X
  default:
    assert(0 && "invalid kind");
    }
}

////////
//
// Simple hash functions for various kinds of Exprs
//
///////

unsigned Expr::computeHash() {
  unsigned res = getKind() * Expr::MAGIC_HASH_CONSTANT;

  int n = getNumKids();
  for (int i = 0; i < n; i++) {
    res <<= 1;
    res ^= getKid(i)->hash() * Expr::MAGIC_HASH_CONSTANT;
  }

  hashValue = res;
  return hashValue;
}

unsigned ConstantExpr::computeHash() {
  hashValue = hash_value(value) ^ (getWidth() * MAGIC_HASH_CONSTANT);
  return hashValue;
}

unsigned CastExpr::computeHash() {
  unsigned res = getWidth() * Expr::MAGIC_HASH_CONSTANT;
  hashValue = res ^ src->hash() * Expr::MAGIC_HASH_CONSTANT;
  return hashValue;
}

unsigned ExtractExpr::computeHash() {
  unsigned res = offset * Expr::MAGIC_HASH_CONSTANT;
  res ^= getWidth() * Expr::MAGIC_HASH_CONSTANT;
  hashValue = res ^ expr->hash() * Expr::MAGIC_HASH_CONSTANT;
  return hashValue;
}

unsigned ReadExpr::computeHash() {
  unsigned res = index->hash() * Expr::MAGIC_HASH_CONSTANT;
  res ^= updates.hash();
  hashValue = res;
  return hashValue;
}

unsigned NotExpr::computeHash() {
  hashValue = expr->hash() * Expr::MAGIC_HASH_CONSTANT * Expr::Not;
  return hashValue;
}

ref<Expr> Expr::createFromKind(Kind k, std::vector<CreateArg> args) {
  unsigned numArgs = args.size();
  (void) numArgs;

  switch(k) {
    case Constant:
    case Extract:
    case Read:
    default:
      assert(0 && "invalid kind");

    case NotOptimized:
      assert(numArgs == 1 && args[0].isExpr() &&
             "invalid args array for given opcode");
      return NotOptimizedExpr::create(args[0].expr);

    case Select:
      assert(numArgs == 3 && args[0].isExpr() &&
             args[1].isExpr() && args[2].isExpr() &&
             "invalid args array for Select opcode");
      return SelectExpr::create(args[0].expr,
                                args[1].expr,
                                args[2].expr);

    case Concat: {
      assert(numArgs == 2 && args[0].isExpr() && args[1].isExpr() &&
             "invalid args array for Concat opcode");

      return ConcatExpr::create(args[0].expr, args[1].expr);
    }

#define CAST_EXPR_CASE(T)                                    \
      case T:                                                \
        assert(numArgs == 2 &&				     \
               args[0].isExpr() && args[1].isWidth() &&      \
               "invalid args array for given opcode");       \
      return T ## Expr::create(args[0].expr, args[1].width); \

#define BINARY_EXPR_CASE(T)                                 \
      case T:                                               \
        assert(numArgs == 2 &&                              \
               args[0].isExpr() && args[1].isExpr() &&      \
               "invalid args array for given opcode");      \
      return T ## Expr::create(args[0].expr, args[1].expr); \

      CAST_EXPR_CASE(ZExt);
      CAST_EXPR_CASE(SExt);

      BINARY_EXPR_CASE(Add);
      BINARY_EXPR_CASE(Sub);
      BINARY_EXPR_CASE(Mul);
      BINARY_EXPR_CASE(UDiv);
      BINARY_EXPR_CASE(SDiv);
      BINARY_EXPR_CASE(URem);
      BINARY_EXPR_CASE(SRem);
      BINARY_EXPR_CASE(And);
      BINARY_EXPR_CASE(Or);
      BINARY_EXPR_CASE(Xor);
      BINARY_EXPR_CASE(Shl);
      BINARY_EXPR_CASE(LShr);
      BINARY_EXPR_CASE(AShr);

      BINARY_EXPR_CASE(Eq);
      BINARY_EXPR_CASE(Ne);
      BINARY_EXPR_CASE(Ult);
      BINARY_EXPR_CASE(Ule);
      BINARY_EXPR_CASE(Ugt);
      BINARY_EXPR_CASE(Uge);
      BINARY_EXPR_CASE(Slt);
      BINARY_EXPR_CASE(Sle);
      BINARY_EXPR_CASE(Sgt);
      BINARY_EXPR_CASE(Sge);
  }
}

UpdateNode* recDeserializeUpdateNode(const ProtoUpdateNode& node) {
    UpdateNode* next = node.has_next() ? recDeserializeUpdateNode(node.next()) : nullptr;
    return new UpdateNode(next, Expr::deserialize(node.updateindex()), Expr::deserialize(node.updatevalue()));
}

UpdateNode* deserializeUpdateNode(CacheUpdateNode::Reader&& reader) {
    UpdateNode* next = reader.hasNext() ? deserializeUpdateNode(reader.getNext()) : nullptr;
    return new UpdateNode(next, Expr::deserialize(reader.getUpdateIndex()), Expr::deserialize(reader.getUpdateValue()));
}

ref<Expr> Expr::deserialize(const CacheExpr::Reader&& re) {
#define KID(x) re.getKids()[x]

    switch (re.getKind()) {
        case Constant: {

            assert(re.getSpecialData().hasConstData() && "ConstExpr does not have constant data");
            auto constData = re.getSpecialData().getConstData();
            if(constData.getConstExprWidth() > 64) {
                APInt i(constData.getConstExprWidth(), constData.getConstExprOverSizeVal().cStr(), 16);
                return new ConstantExpr(i);
            }
            return ConstantExpr::create(constData.getConstExprVal(), constData.getConstExprWidth());
        }
        case Select: {
            assert(re.getKids().size() == 3 && "Wrong number of children for SelectExpr");
            return SelectExpr::create(Expr::deserialize(KID(0)), Expr::deserialize(KID(1)),
                                      Expr::deserialize(KID(2)));
        }
        case Extract: {
            assert(re.getSpecialData().hasExtractData() && "ExtractExpr does not have extract data");
            auto extractData = re.getSpecialData().getExtractData();
            auto thing = deserialize(extractData.getExpr());
            return ExtractExpr::create(thing,
                                       extractData.getExtractBitOff(),
                                       extractData.getExtractWidth());
        }
        case Read: {
            assert(re.getSpecialData().hasReadData() && "ReadExpr does not have read data");
            const auto &readData = re.getSpecialData().getReadData();
            UpdateNode* head = nullptr;
            if(readData.hasHead()) {
                head = deserializeUpdateNode(readData.getHead());
            }
            auto index = deserialize(readData.getExpr());
            return ReadExpr::create(UpdateList(Array::deserialize(readData.getRoot()), head), index);
            break;
        }

        case Concat: {
            assert(re.getKids().size() >= 2);
            if (re.getKids().size() == 2) {
                return ConcatExpr::create(Expr::deserialize(KID(0)), Expr::deserialize(KID(1)));
            } else if (re.getKids().size() == 4) {
                return ConcatExpr::create4(Expr::deserialize(KID(0)), Expr::deserialize(KID(1)),
                                           Expr::deserialize(KID(2)), Expr::deserialize(KID(3)));
            } else if (re.getKids().size() == 8) {
                return ConcatExpr::create8(Expr::deserialize(KID(0)), Expr::deserialize(KID(1)),
                                           Expr::deserialize(KID(2)), Expr::deserialize(KID(3)),
                                           Expr::deserialize(KID(4)), Expr::deserialize(KID(5)),
                                           Expr::deserialize(KID(6)), Expr::deserialize(KID(7)));
            }
            assert(false && "ConcatExpr Number of kids was not 2 or 4 or 8");
        }
        case NotOptimized:
            assert(re.getSpecialData().hasNOData() && "NotOptimizedExpr does not have NotOptimizedData");
            return NotOptimizedExpr::create(deserialize(re.getSpecialData().getNOData().getSrc()));

#define CAP_CAST_EXPR_CASE(T)                                    \
      case T:                                                \
        return T ## Expr::create(Expr::deserialize(KID(0)), re.getWidth()); \

#define CAP_BINARY_EXPR_CASE(T)                                 \
      case T:                       \
        if(re.getKids().size() != 2)    { \
            printKind(errs(), (Kind)re.getKind()); \
            return ref<Expr>(); \
        } \
        return T ## Expr::create(Expr::deserialize(KID(0)), Expr::deserialize(KID(1)));

        CAP_CAST_EXPR_CASE(ZExt);
        CAP_CAST_EXPR_CASE(SExt);

        CAP_BINARY_EXPR_CASE(Add);
        CAP_BINARY_EXPR_CASE(Sub);
        CAP_BINARY_EXPR_CASE(Mul);
        CAP_BINARY_EXPR_CASE(UDiv);
        CAP_BINARY_EXPR_CASE(SDiv);
        CAP_BINARY_EXPR_CASE(URem);
        CAP_BINARY_EXPR_CASE(SRem);
        CAP_BINARY_EXPR_CASE(And);
        CAP_BINARY_EXPR_CASE(Or);
        CAP_BINARY_EXPR_CASE(Xor);
        CAP_BINARY_EXPR_CASE(Shl);
        CAP_BINARY_EXPR_CASE(LShr);
        CAP_BINARY_EXPR_CASE(AShr);

        CAP_BINARY_EXPR_CASE(Eq);
        CAP_BINARY_EXPR_CASE(Ne);
        CAP_BINARY_EXPR_CASE(Ult);
        CAP_BINARY_EXPR_CASE(Ule);
        CAP_BINARY_EXPR_CASE(Ugt);
        CAP_BINARY_EXPR_CASE(Uge);
        CAP_BINARY_EXPR_CASE(Slt);
        CAP_BINARY_EXPR_CASE(Sle);
        CAP_BINARY_EXPR_CASE(Sgt);
        CAP_BINARY_EXPR_CASE(Sge);
        default:
            std::cout << "Kind: " << (Kind) re.getKind() << std::endl;

            return ref<Expr>();

    }
}

ref<Expr> Expr::deserialize(const ProtoExpr &pe) {
    switch (pe.kind()) {
        case Constant: {
            assert(pe.has_constdata() && "ConstExpr does not have constant data");
            return ConstantExpr::create(pe.constdata().constexprval(), pe.constdata().constexprbwidth());
        }
        case Select: {
            assert(pe.kids_size() == 3 && "Wrong number of children for SelectExpr");
            return SelectExpr::create(Expr::deserialize(pe.kids(0)), Expr::deserialize(pe.kids(1)),
                                      Expr::deserialize(pe.kids(2)));
        }
        case Extract: {
            assert(pe.has_extractdata() && "ExtractExpr does not have extract data");
            auto thing = deserialize(pe.extractdata().expr());
            return ExtractExpr::create(thing,
                                       pe.extractdata().extractbitoff(),
                                       pe.extractdata().extractwidth());
        }
        case Read: {
            assert(pe.has_readdata() && "ReadExpr does not have read data");
            const auto &readData = pe.readdata();
            UpdateNode* head = nullptr;
            if(readData.has_head()) {
                head = recDeserializeUpdateNode(readData.head());
            }
            auto index = deserialize(readData.expr());
            return ReadExpr::create(UpdateList(Array::deserialize(readData.root()), head), index);
        }

        case Concat: {
            assert(pe.kids().size() >= 2);
            if (pe.kids().size() == 2) {
                return ConcatExpr::create(Expr::deserialize(pe.kids(0)), Expr::deserialize(pe.kids(1)));
            } else if (pe.kids().size() == 4) {
                return ConcatExpr::create4(Expr::deserialize(pe.kids(0)), Expr::deserialize(pe.kids(1)),
                                           Expr::deserialize(pe.kids(2)), Expr::deserialize(pe.kids(3)));
            } else if (pe.kids().size() == 8) {
                return ConcatExpr::create8(Expr::deserialize(pe.kids(0)), Expr::deserialize(pe.kids(1)),
                                           Expr::deserialize(pe.kids(2)), Expr::deserialize(pe.kids(3)),
                                           Expr::deserialize(pe.kids(4)), Expr::deserialize(pe.kids(5)),
                                           Expr::deserialize(pe.kids(6)), Expr::deserialize(pe.kids(7)));
            }
            assert(false && "ConcatExpr Number of kids was not 2 or 4 or 8");
        }
        case NotOptimized:
            assert(pe.has_nodata() && "NotOptimizedExpr does not have NotOptimizedData");
            return NotOptimizedExpr::create(deserialize(pe.nodata().src()));

#define PROTO_CAST_EXPR_CASE(T)                                    \
      case T:                                                \
        return T ## Expr::create(Expr::deserialize(pe.kids(0)), pe.width()); \

#define PROTO_BINARY_EXPR_CASE(T)                                 \
      case T:                       \
        if(pe.kids_size() != 2)    { \
            pe.PrintDebugString();  \
            printKind(errs(), (Kind)pe.kind()); \
            return ref<Expr>(); \
        } \
        assert(pe.kids_size() == 2);      \
        return T ## Expr::create(Expr::deserialize(pe.kids(0)), Expr::deserialize(pe.kids(1))); \

        PROTO_CAST_EXPR_CASE(ZExt);
        PROTO_CAST_EXPR_CASE(SExt);

        PROTO_BINARY_EXPR_CASE(Add);
        PROTO_BINARY_EXPR_CASE(Sub);
        PROTO_BINARY_EXPR_CASE(Mul);
        PROTO_BINARY_EXPR_CASE(UDiv);
        PROTO_BINARY_EXPR_CASE(SDiv);
        PROTO_BINARY_EXPR_CASE(URem);
        PROTO_BINARY_EXPR_CASE(SRem);
        PROTO_BINARY_EXPR_CASE(And);
        PROTO_BINARY_EXPR_CASE(Or);
        PROTO_BINARY_EXPR_CASE(Xor);
        PROTO_BINARY_EXPR_CASE(Shl);
        PROTO_BINARY_EXPR_CASE(LShr);
        PROTO_BINARY_EXPR_CASE(AShr);

        PROTO_BINARY_EXPR_CASE(Eq);
        PROTO_BINARY_EXPR_CASE(Ne);
        PROTO_BINARY_EXPR_CASE(Ult);
        PROTO_BINARY_EXPR_CASE(Ule);
        PROTO_BINARY_EXPR_CASE(Ugt);
        PROTO_BINARY_EXPR_CASE(Uge);
        PROTO_BINARY_EXPR_CASE(Slt);
        PROTO_BINARY_EXPR_CASE(Sle);
        PROTO_BINARY_EXPR_CASE(Sgt);
        PROTO_BINARY_EXPR_CASE(Sge);
        default:
            std::cout << "Kind: " << (Kind) pe.kind() << std::endl;

            return ref<Expr>();
    }

    return ref<Expr>();
}

void Expr::printWidth(llvm::raw_ostream &os, Width width) {
  switch(width) {
  case Expr::Bool: os << "Expr::Bool"; break;
  case Expr::Int8: os << "Expr::Int8"; break;
  case Expr::Int16: os << "Expr::Int16"; break;
  case Expr::Int32: os << "Expr::Int32"; break;
  case Expr::Int64: os << "Expr::Int64"; break;
  case Expr::Fl80: os << "Expr::Fl80"; break;
  default: os << "<invalid type: " << (unsigned) width << ">";
  }
}

ref<Expr> Expr::createImplies(ref<Expr> hyp, ref<Expr> conc) {
  return OrExpr::create(Expr::createIsZero(hyp), conc);
}

ref<Expr> Expr::createIsZero(ref<Expr> e) {
  return EqExpr::create(e, ConstantExpr::create(0, e->getWidth()));
}

void Expr::print(llvm::raw_ostream &os) const {
  ExprPPrinter::printSingleExpr(os, const_cast<Expr*>(this));
}

void Expr::dump() const {
  this->print(errs());
  errs() << "\n";
}

/***/

ref<Expr> ConstantExpr::fromMemory(void *address, Width width) {
  switch (width) {
  case  Expr::Bool: return ConstantExpr::create(*(( uint8_t*) address), width);
  case  Expr::Int8: return ConstantExpr::create(*(( uint8_t*) address), width);
  case Expr::Int16: return ConstantExpr::create(*((uint16_t*) address), width);
  case Expr::Int32: return ConstantExpr::create(*((uint32_t*) address), width);
  case Expr::Int64: return ConstantExpr::create(*((uint64_t*) address), width);
  // FIXME: what about machines without x87 support?
  default:
    return ConstantExpr::alloc(llvm::APInt(width,
      (width+llvm::integerPartWidth-1)/llvm::integerPartWidth,
      (const uint64_t*)address));
  }
}

void ConstantExpr::toMemory(void *address) {
  switch (getWidth()) {
  default: assert(0 && "invalid type");
  case  Expr::Bool: *(( uint8_t*) address) = getZExtValue(1); break;
  case  Expr::Int8: *(( uint8_t*) address) = getZExtValue(8); break;
  case Expr::Int16: *((uint16_t*) address) = getZExtValue(16); break;
  case Expr::Int32: *((uint32_t*) address) = getZExtValue(32); break;
  case Expr::Int64: *((uint64_t*) address) = getZExtValue(64); break;
  // FIXME: what about machines without x87 support?
  case Expr::Fl80:
    *((long double*) address) = *(const long double*) value.getRawData();
    break;
  }
}

ProtoExpr* ConstantExpr::serialize() const {
    ProtoExpr* pe = Expr::serialize();
    auto * pc = new ProtoConstExpr();
    pc->set_constexprbwidth(this->value.getBitWidth());
    pc->set_constexprval(this->value.getZExtValue());
    pe->set_width(getWidth());
    pe->set_allocated_constdata(pc);
    return pe;
}

void ConstantExpr::serialize(CacheExpr::Builder&& builder) const {
    Expr::serialize(std::forward<CacheExpr::Builder>(builder));
    CacheConstExpr::Builder constB = builder.initSpecialData().initConstData();
    constB.setConstExprWidth(this->value.getBitWidth());
    if(this->value.getBitWidth() > 64L) {
        constB.setConstExprOverSizeVal(this->value.toString(16, true));
    } else {
        constB.setConstExprVal(this->value.getZExtValue());
    }
}
void ConstantExpr::toString(std::string &Res, unsigned radix) const {
  Res = value.toString(radix, false);
}

ref<ConstantExpr> ConstantExpr::Concat(const ref<ConstantExpr> &RHS) {
  Expr::Width W = getWidth() + RHS->getWidth();
  APInt Tmp(value);
  Tmp=Tmp.zext(W);
  Tmp <<= RHS->getWidth();
  Tmp |= APInt(RHS->value).zext(W);

  return ConstantExpr::alloc(Tmp);
}

ref<ConstantExpr> ConstantExpr::Extract(unsigned Offset, Width W) {
  return ConstantExpr::alloc(APInt(value.ashr(Offset)).zextOrTrunc(W));
}

ref<ConstantExpr> ConstantExpr::ZExt(Width W) {
  return ConstantExpr::alloc(APInt(value).zextOrTrunc(W));
}

ref<ConstantExpr> ConstantExpr::SExt(Width W) {
  return ConstantExpr::alloc(APInt(value).sextOrTrunc(W));
}

ref<ConstantExpr> ConstantExpr::Add(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value + RHS->value);
}

ref<ConstantExpr> ConstantExpr::Neg() {
  return ConstantExpr::alloc(-value);
}

ref<ConstantExpr> ConstantExpr::Sub(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value - RHS->value);
}

ref<ConstantExpr> ConstantExpr::Mul(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value * RHS->value);
}

ref<ConstantExpr> ConstantExpr::UDiv(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.udiv(RHS->value));
}

ref<ConstantExpr> ConstantExpr::SDiv(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.sdiv(RHS->value));
}

ref<ConstantExpr> ConstantExpr::URem(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.urem(RHS->value));
}

ref<ConstantExpr> ConstantExpr::SRem(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.srem(RHS->value));
}

ref<ConstantExpr> ConstantExpr::And(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value & RHS->value);
}

ref<ConstantExpr> ConstantExpr::Or(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value | RHS->value);
}

ref<ConstantExpr> ConstantExpr::Xor(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value ^ RHS->value);
}

ref<ConstantExpr> ConstantExpr::Shl(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.shl(RHS->value));
}

ref<ConstantExpr> ConstantExpr::LShr(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.lshr(RHS->value));
}

ref<ConstantExpr> ConstantExpr::AShr(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.ashr(RHS->value));
}

ref<ConstantExpr> ConstantExpr::Not() {
  return ConstantExpr::alloc(~value);
}

ref<ConstantExpr> ConstantExpr::Eq(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value == RHS->value, Expr::Bool);
}

ref<ConstantExpr> ConstantExpr::Ne(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value != RHS->value, Expr::Bool);
}

ref<ConstantExpr> ConstantExpr::Ult(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.ult(RHS->value), Expr::Bool);
}

ref<ConstantExpr> ConstantExpr::Ule(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.ule(RHS->value), Expr::Bool);
}

ref<ConstantExpr> ConstantExpr::Ugt(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.ugt(RHS->value), Expr::Bool);
}

ref<ConstantExpr> ConstantExpr::Uge(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.uge(RHS->value), Expr::Bool);
}

ref<ConstantExpr> ConstantExpr::Slt(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.slt(RHS->value), Expr::Bool);
}

ref<ConstantExpr> ConstantExpr::Sle(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.sle(RHS->value), Expr::Bool);
}

ref<ConstantExpr> ConstantExpr::Sgt(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.sgt(RHS->value), Expr::Bool);
}

ref<ConstantExpr> ConstantExpr::Sge(const ref<ConstantExpr> &RHS) {
  return ConstantExpr::alloc(value.sge(RHS->value), Expr::Bool);
}

/***/

ref<Expr>  NotOptimizedExpr::create(ref<Expr> src) {
  return NotOptimizedExpr::alloc(src);
}

ProtoExpr* NotOptimizedExpr::serialize() const {
    ProtoExpr* base = Expr::serialize();
    ProtoNotOptimizedExpr* pne = new ProtoNotOptimizedExpr;
    pne->set_allocated_src(src->serialize());
    base->set_allocated_nodata(pne);
    return base;
}
void NotOptimizedExpr::serialize(CacheExpr::Builder &&builder) const {
    Expr::serialize(std::forward<CacheExpr::Builder>(builder));
    src->serialize(builder.initSpecialData().initNOData().initSrc());
}
/***/

void Array::serialize(CacheArray::Builder&& builder) const {
    builder.setDomain(this->domain);
    builder.setRange(this->range);
    builder.setName(this->name);
    builder.setSize(this->size);
    if(this->constantValues.empty())
        return;
    auto consts = builder.initConstantValues(this->constantValues.size());
    int i = 0;
    for(; i < this->constantValues.size(); i++) {
        constantValues[i]->serialize(consts[i]);
    }
}
ArrayCache ac;
const Array* Array::deserialize(const CacheArray::Reader&& reader) {
    if(!reader.getConstantValues().size()) {
        return ac.CreateArray(reader.getName(), reader.getSize(), nullptr, nullptr, reader.getDomain(), reader.getRange());
    }
    std::vector<ref<ConstantExpr>> cVals;
    for(const auto& cvalue : reader.getConstantValues()) {
        auto constData = cvalue.getSpecialData().getConstData();
        ref<ConstantExpr> cExpr =  ConstantExpr::create(constData.getConstExprVal(), constData.getConstExprWidth());
        cVals.push_back(cExpr);
    }
    return ac.CreateArray(reader.getName(), reader.getSize(), &cVals[0], &cVals[0] + reader.getSize(), reader.getDomain(), reader.getRange());

}
Array::Array(const std::string &_name, uint64_t _size,
             const ref<ConstantExpr> *constantValuesBegin,
             const ref<ConstantExpr> *constantValuesEnd, Expr::Width _domain,
             Expr::Width _range)
    : name(_name), size(_size), domain(_domain), range(_range),
      constantValues(constantValuesBegin, constantValuesEnd) {

  assert((isSymbolicArray() || constantValues.size() == size) &&
         "Invalid size for constant array!");
  computeHash();
#ifndef NDEBUG
  for (const ref<ConstantExpr> *it = constantValuesBegin;
       it != constantValuesEnd; ++it)
    assert((*it)->getWidth() == getRange() &&
           "Invalid initial constant value!");
#endif // NDEBUG
}

const Array* Array::deserialize(const ProtoArray& protoArray) {
    if(protoArray.constantvalues().empty()) {
        return ac.CreateArray(protoArray.name(), protoArray.size(), nullptr, nullptr, protoArray.domain(), protoArray.range());
    }
    std::vector<ref<ConstantExpr>> cVals;
    for(const auto& cvalue : protoArray.constantvalues()) {
        ref<ConstantExpr> cExpr =  ConstantExpr::create(cvalue.constdata().constexprval(), cvalue.constdata().constexprbwidth());
        cVals.push_back(cExpr);
    }
    return ac.CreateArray(protoArray.name(), protoArray.size(), &cVals[0], &cVals[0] + protoArray.size(), protoArray.domain(), protoArray.range());
}

bool Array::operator==(const Array& rhs) const {
    if(rhs.size != this->size) {
       return false;
    }
    if(rhs.range != this->range) {
        return false;
    }
    if(rhs.domain != this->domain) {
        return false;
    }
    if(rhs.name != this->name) {
        return false;
    }

    for(unsigned i = 0; i < constantValues.size(); i++) {
        if(rhs.constantValues.at(i) != constantValues.at(i)) {
            return false;
        }
    }
    return true;
}

bool Array::operator!=(const Array &rhs) const {
    return !(*this == rhs);
}

Array::~Array() = default;

ProtoArray *Array::serialize() const {
    auto *protoArray = new ProtoArray();
    protoArray->set_name(getName());
    protoArray->set_size(getSize());
    protoArray->set_range(getRange());
    protoArray->set_domain(getDomain());
    for (const auto &constExpr : this->constantValues) {
        protoArray->mutable_constantvalues()->AddAllocated(constExpr->serialize());
    }
    //const Array *pArray = Array::deserialize(*protoArray);
    /*assert(pArray->hash() == this->hash());
    for(unsigned i = 0; i < this->constantValues.size(); i++) {
        assert(pArray->constantValues[i].compare(this->constantValues[i]) == 0);
    }*/
    return protoArray;
}

unsigned Array::computeHash() {
  unsigned res = 0;
  for (unsigned i = 0, e = name.size(); i != e; ++i)
    res = (res * Expr::MAGIC_HASH_CONSTANT) + name[i];
  res = (res * Expr::MAGIC_HASH_CONSTANT) + size;
  hashValue = res;
  return hashValue;
}
/***/

ref<Expr> ReadExpr::create(const UpdateList &ul, ref<Expr> index) {
  // rollback index when possible...

  // XXX this doesn't really belong here... there are basically two
  // cases, one is rebuild, where we want to optimistically try various
  // optimizations when the index has changed, and the other is
  // initial creation, where we expect the ObjectState to have constructed
  // a smart UpdateList so it is not worth rescanning.

  const UpdateNode *un = ul.head;
  bool updateListHasSymbolicWrites = false;
  for (; un; un=un->next) {
    ref<Expr> cond = EqExpr::create(index, un->index);

    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(cond)) {
      if (CE->isTrue())
        return un->value;
    } else {
      updateListHasSymbolicWrites = true;
      break;
    }
  }

  if (ul.root->isConstantArray() && !updateListHasSymbolicWrites) {
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(index)) {
      assert(CE->getWidth() <= 64 && "Index too large");
      uint64_t concreteIndex = CE->getZExtValue();
      uint64_t size = ul.root->size;
      if (concreteIndex < size) {
        return ul.root->constantValues[concreteIndex];
      }
    }
  }

  return ReadExpr::alloc(ul, index);
}

int ReadExpr::compareContents(const Expr &b) const {
  return updates.compare(static_cast<const ReadExpr&>(b).updates);
}

ProtoUpdateNode* serializeList(const UpdateNode* iter) {
    auto* pn = new ProtoUpdateNode;
    pn->set_allocated_updateindex(iter->index->serialize());
    pn->set_allocated_updatevalue(iter->value->serialize());
    if(iter->next)
        pn->set_allocated_next(serializeList(iter->next));
    return pn;
}

void serializeList(CacheUpdateNode::Builder&& builder, const UpdateNode* iter) {
    iter->value->serialize(builder.initUpdateValue());
    iter->index->serialize(builder.initUpdateIndex());
    if(iter->next)
        serializeList(builder.initNext(), iter->next);
}

ProtoExpr* ReadExpr::serialize() const {
    ProtoExpr* base = Expr::serialize();
    auto* readExpr = new ProtoReadExpr();
    readExpr->set_allocated_expr(this->index->serialize());
    readExpr->set_allocated_root(this->updates.root->serialize());
    const UpdateNode* iter = this->updates.head;
    if(iter)
        readExpr->set_allocated_head(serializeList(iter));
    base->set_allocated_readdata(readExpr);
    return base;
}

void ReadExpr::serialize(CacheExpr::Builder &&builder) const {
    Expr::serialize(std::forward<CacheExpr::Builder>(builder));
    auto readData = builder.initSpecialData().initReadData();
    index->serialize(readData.initExpr());
    updates.root->serialize(readData.initRoot());
    const UpdateNode* iter = this->updates.head;
    if(iter)
        serializeList(readData.initHead(), iter);//->set_allocated_head(serializeList(iter));
}
/***/
ref<Expr> SelectExpr::create(ref<Expr> c, ref<Expr> t, ref<Expr> f) {
  Expr::Width kt = t->getWidth();

  assert(c->getWidth()==Bool && "type mismatch");
  assert(kt==f->getWidth() && "type mismatch");

  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(c)) {
    return CE->isTrue() ? t : f;
  } else if (t==f) {
    return t;
  } else if (kt==Expr::Bool) { // c ? t : f  <=> (c and t) or (not c and f)
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(t)) {
      if (CE->isTrue()) {
        return OrExpr::create(c, f);
      } else {
        return AndExpr::create(Expr::createIsZero(c), f);
      }
    } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(f)) {
      if (CE->isTrue()) {
        return OrExpr::create(Expr::createIsZero(c), t);
      } else {
        return AndExpr::create(c, t);
      }
    }
  }

  return SelectExpr::alloc(c, t, f);
}

/***/

ref<Expr> ConcatExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  Expr::Width w = l->getWidth() + r->getWidth();

  // Fold concatenation of constants.
  //
  // FIXME: concat 0 x -> zext x ?
  if (ConstantExpr *lCE = dyn_cast<ConstantExpr>(l))
    if (ConstantExpr *rCE = dyn_cast<ConstantExpr>(r))
      return lCE->Concat(rCE);

  // Merge contiguous Extracts
  if (ExtractExpr *ee_left = dyn_cast<ExtractExpr>(l)) {
    if (ExtractExpr *ee_right = dyn_cast<ExtractExpr>(r)) {
      if (ee_left->expr == ee_right->expr &&
          ee_right->offset + ee_right->width == ee_left->offset) {
        return ExtractExpr::create(ee_left->expr, ee_right->offset, w);
      }
    }
  }

  return ConcatExpr::alloc(l, r);
}

/// Shortcut to concat N kids.  The chain returned is unbalanced to the right
ref<Expr> ConcatExpr::createN(unsigned n_kids, const ref<Expr> kids[]) {
  assert(n_kids > 0);
  if (n_kids == 1)
    return kids[0];

  ref<Expr> r = ConcatExpr::create(kids[n_kids-2], kids[n_kids-1]);
  for (int i=n_kids-3; i>=0; i--)
    r = ConcatExpr::create(kids[i], r);
  return r;
}

/// Shortcut to concat 4 kids.  The chain returned is unbalanced to the right
ref<Expr> ConcatExpr::create4(const ref<Expr> &kid1, const ref<Expr> &kid2,
                              const ref<Expr> &kid3, const ref<Expr> &kid4) {
  return ConcatExpr::create(kid1, ConcatExpr::create(kid2, ConcatExpr::create(kid3, kid4)));
}

/// Shortcut to concat 8 kids.  The chain returned is unbalanced to the right
ref<Expr> ConcatExpr::create8(const ref<Expr> &kid1, const ref<Expr> &kid2,
			      const ref<Expr> &kid3, const ref<Expr> &kid4,
			      const ref<Expr> &kid5, const ref<Expr> &kid6,
			      const ref<Expr> &kid7, const ref<Expr> &kid8) {
  return ConcatExpr::create(kid1, ConcatExpr::create(kid2, ConcatExpr::create(kid3,
			      ConcatExpr::create(kid4, ConcatExpr::create4(kid5, kid6, kid7, kid8)))));
}

/***/

ref<Expr> ExtractExpr::create(ref<Expr> expr, unsigned off, Width w) {
  unsigned kw = expr->getWidth();
  assert(w > 0 && off + w <= kw && "invalid extract");

  if (w == kw) {
    return expr;
  } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(expr)) {
    return CE->Extract(off, w);
  } else {
    // Extract(Concat)
    if (ConcatExpr *ce = dyn_cast<ConcatExpr>(expr)) {
      // if the extract skips the right side of the concat
      if (off >= ce->getRight()->getWidth())
	return ExtractExpr::create(ce->getLeft(), off - ce->getRight()->getWidth(), w);

      // if the extract skips the left side of the concat
      if (off + w <= ce->getRight()->getWidth())
	return ExtractExpr::create(ce->getRight(), off, w);

      // E(C(x,y)) = C(E(x), E(y))
      return ConcatExpr::create(ExtractExpr::create(ce->getKid(0), 0, w - ce->getKid(1)->getWidth() + off),
				ExtractExpr::create(ce->getKid(1), off, ce->getKid(1)->getWidth() - off));
    }
  }

  return ExtractExpr::alloc(expr, off, w);
}

ProtoExpr * ExtractExpr::serialize() const {
    ProtoExpr* base = Expr::serialize();
    auto* extractExpr = new ProtoExtractExpr();
    extractExpr->set_extractbitoff(this->offset);
    extractExpr->set_extractwidth(this->width);
    extractExpr->set_allocated_expr(expr->serialize());
    base->set_allocated_extractdata(extractExpr);
    return base;
}

void ExtractExpr::serialize(CacheExpr::Builder &&builder) const {
    Expr::serialize(std::forward<CacheExpr::Builder>(builder));
    auto exprData = builder.initSpecialData().initExtractData();
    exprData.setExtractBitOff(this->offset);
    exprData.setExtractWidth(this->width);
    this->expr->serialize(exprData.initExpr());
}

/***/

ref<Expr> NotExpr::create(const ref<Expr> &e) {
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(e))
    return CE->Not();

  return NotExpr::alloc(e);
}


/***/

ref<Expr> ZExtExpr::create(const ref<Expr> &e, Width w) {
  unsigned kBits = e->getWidth();
  if (w == kBits) {
    return e;
  } else if (w < kBits) { // trunc
    return ExtractExpr::create(e, 0, w);
  } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(e)) {
    return CE->ZExt(w);
  } else {
    return ZExtExpr::alloc(e, w);
  }
}

ref<Expr> SExtExpr::create(const ref<Expr> &e, Width w) {
  unsigned kBits = e->getWidth();
  if (w == kBits) {
    return e;
  } else if (w < kBits) { // trunc
    return ExtractExpr::create(e, 0, w);
  } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(e)) {
    return CE->SExt(w);
  } else {
    return SExtExpr::alloc(e, w);
  }
}

/***/

static ref<Expr> AndExpr_create(Expr *l, Expr *r);
static ref<Expr> XorExpr_create(Expr *l, Expr *r);

static ref<Expr> EqExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr);
static ref<Expr> AndExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r);
static ref<Expr> SubExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r);
static ref<Expr> XorExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r);

static ref<Expr> AddExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r) {
  Expr::Width type = cl->getWidth();

  if (type==Expr::Bool) {
    return XorExpr_createPartialR(cl, r);
  } else if (cl->isZero()) {
    return r;
  } else {
    Expr::Kind rk = r->getKind();
    if (rk==Expr::Add && isa<ConstantExpr>(r->getKid(0))) { // A + (B+c) == (A+B) + c
      return AddExpr::create(AddExpr::create(cl, r->getKid(0)),
                             r->getKid(1));
    } else if (rk==Expr::Sub && isa<ConstantExpr>(r->getKid(0))) { // A + (B-c) == (A+B) - c
      return SubExpr::create(AddExpr::create(cl, r->getKid(0)),
                             r->getKid(1));
    } else {
      return AddExpr::alloc(cl, r);
    }
  }
}
static ref<Expr> AddExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr) {
  return AddExpr_createPartialR(cr, l);
}
static ref<Expr> AddExpr_create(Expr *l, Expr *r) {
  Expr::Width type = l->getWidth();

  if (type == Expr::Bool) {
    return XorExpr_create(l, r);
  } else {
    Expr::Kind lk = l->getKind(), rk = r->getKind();
    if (lk==Expr::Add && isa<ConstantExpr>(l->getKid(0))) { // (k+a)+b = k+(a+b)
      return AddExpr::create(l->getKid(0),
                             AddExpr::create(l->getKid(1), r));
    } else if (lk==Expr::Sub && isa<ConstantExpr>(l->getKid(0))) { // (k-a)+b = k+(b-a)
      return AddExpr::create(l->getKid(0),
                             SubExpr::create(r, l->getKid(1)));
    } else if (rk==Expr::Add && isa<ConstantExpr>(r->getKid(0))) { // a + (k+b) = k+(a+b)
      return AddExpr::create(r->getKid(0),
                             AddExpr::create(l, r->getKid(1)));
    } else if (rk==Expr::Sub && isa<ConstantExpr>(r->getKid(0))) { // a + (k-b) = k+(a-b)
      return AddExpr::create(r->getKid(0),
                             SubExpr::create(l, r->getKid(1)));
    } else {
      return AddExpr::alloc(l, r);
    }
  }
}

static ref<Expr> SubExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r) {
  Expr::Width type = cl->getWidth();

  if (type==Expr::Bool) {
    return XorExpr_createPartialR(cl, r);
  } else {
    Expr::Kind rk = r->getKind();
    if (rk==Expr::Add && isa<ConstantExpr>(r->getKid(0))) { // A - (B+c) == (A-B) - c
      return SubExpr::create(SubExpr::create(cl, r->getKid(0)),
                             r->getKid(1));
    } else if (rk==Expr::Sub && isa<ConstantExpr>(r->getKid(0))) { // A - (B-c) == (A-B) + c
      return AddExpr::create(SubExpr::create(cl, r->getKid(0)),
                             r->getKid(1));
    } else {
      return SubExpr::alloc(cl, r);
    }
  }
}
static ref<Expr> SubExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr) {
  // l - c => l + (-c)
  return AddExpr_createPartial(l,
                               ConstantExpr::alloc(0, cr->getWidth())->Sub(cr));
}
static ref<Expr> SubExpr_create(Expr *l, Expr *r) {
  Expr::Width type = l->getWidth();

  if (type == Expr::Bool) {
    return XorExpr_create(l, r);
  } else if (*l==*r) {
    return ConstantExpr::alloc(0, type);
  } else {
    Expr::Kind lk = l->getKind(), rk = r->getKind();
    if (lk==Expr::Add && isa<ConstantExpr>(l->getKid(0))) { // (k+a)-b = k+(a-b)
      return AddExpr::create(l->getKid(0),
                             SubExpr::create(l->getKid(1), r));
    } else if (lk==Expr::Sub && isa<ConstantExpr>(l->getKid(0))) { // (k-a)-b = k-(a+b)
      return SubExpr::create(l->getKid(0),
                             AddExpr::create(l->getKid(1), r));
    } else if (rk==Expr::Add && isa<ConstantExpr>(r->getKid(0))) { // a - (k+b) = (a-c) - k
      return SubExpr::create(SubExpr::create(l, r->getKid(1)),
                             r->getKid(0));
    } else if (rk==Expr::Sub && isa<ConstantExpr>(r->getKid(0))) { // a - (k-b) = (a+b) - k
      return SubExpr::create(AddExpr::create(l, r->getKid(1)),
                             r->getKid(0));
    } else {
      return SubExpr::alloc(l, r);
    }
  }
}

static ref<Expr> MulExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r) {
  Expr::Width type = cl->getWidth();

  if (type == Expr::Bool) {
    return AndExpr_createPartialR(cl, r);
  } else if (cl->isOne()) {
    return r;
  } else if (cl->isZero()) {
    return cl;
  } else {
    return MulExpr::alloc(cl, r);
  }
}
static ref<Expr> MulExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr) {
  return MulExpr_createPartialR(cr, l);
}
static ref<Expr> MulExpr_create(Expr *l, Expr *r) {
  Expr::Width type = l->getWidth();

  if (type == Expr::Bool) {
    return AndExpr::alloc(l, r);
  } else {
    return MulExpr::alloc(l, r);
  }
}

static ref<Expr> AndExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr) {
  if (cr->isAllOnes()) {
    return l;
  } else if (cr->isZero()) {
    return cr;
  } else {
    return AndExpr::alloc(l, cr);
  }
}
static ref<Expr> AndExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r) {
  return AndExpr_createPartial(r, cl);
}
static ref<Expr> AndExpr_create(Expr *l, Expr *r) {
  return AndExpr::alloc(l, r);
}

static ref<Expr> OrExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr) {
  if (cr->isAllOnes()) {
    return cr;
  } else if (cr->isZero()) {
    return l;
  } else {
    return OrExpr::alloc(l, cr);
  }
}
static ref<Expr> OrExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r) {
  return OrExpr_createPartial(r, cl);
}
static ref<Expr> OrExpr_create(Expr *l, Expr *r) {
  return OrExpr::alloc(l, r);
}

static ref<Expr> XorExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r) {
  if (cl->isZero()) {
    return r;
  } else if (cl->getWidth() == Expr::Bool) {
    return EqExpr_createPartial(r, ConstantExpr::create(0, Expr::Bool));
  } else {
    return XorExpr::alloc(cl, r);
  }
}

static ref<Expr> XorExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr) {
  return XorExpr_createPartialR(cr, l);
}
static ref<Expr> XorExpr_create(Expr *l, Expr *r) {
  return XorExpr::alloc(l, r);
}

static ref<Expr> UDivExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return l;
  } else{
    return UDivExpr::alloc(l, r);
  }
}

static ref<Expr> SDivExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return l;
  } else{
    return SDivExpr::alloc(l, r);
  }
}

static ref<Expr> URemExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return ConstantExpr::create(0, Expr::Bool);
  } else{
    return URemExpr::alloc(l, r);
  }
}

static ref<Expr> SRemExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return ConstantExpr::create(0, Expr::Bool);
  } else{
    return SRemExpr::alloc(l, r);
  }
}

static ref<Expr> ShlExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // l & !r
    return AndExpr::create(l, Expr::createIsZero(r));
  } else{
    return ShlExpr::alloc(l, r);
  }
}

static ref<Expr> LShrExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // l & !r
    return AndExpr::create(l, Expr::createIsZero(r));
  } else{
    return LShrExpr::alloc(l, r);
  }
}

static ref<Expr> AShrExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // l
    return l;
  } else{
    return AShrExpr::alloc(l, r);
  }
}

#define BCREATE_R(_e_op, _op, partialL, partialR) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r) { \
  assert(l->getWidth()==r->getWidth() && "type mismatch");              \
  if (ConstantExpr *cl = dyn_cast<ConstantExpr>(l)) {                   \
    if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r))                   \
      return cl->_op(cr);                                               \
    return _e_op ## _createPartialR(cl, r.get());                       \
  } else if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r)) {            \
    return _e_op ## _createPartial(l.get(), cr);                        \
  }                                                                     \
  return _e_op ## _create(l.get(), r.get());                            \
}

#define BCREATE(_e_op, _op) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r) { \
  assert(l->getWidth()==r->getWidth() && "type mismatch");          \
  if (ConstantExpr *cl = dyn_cast<ConstantExpr>(l))                 \
    if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r))               \
      return cl->_op(cr);                                           \
  return _e_op ## _create(l, r);                                    \
}

BCREATE_R(AddExpr, Add, AddExpr_createPartial, AddExpr_createPartialR)
BCREATE_R(SubExpr, Sub, SubExpr_createPartial, SubExpr_createPartialR)
BCREATE_R(MulExpr, Mul, MulExpr_createPartial, MulExpr_createPartialR)
BCREATE_R(AndExpr, And, AndExpr_createPartial, AndExpr_createPartialR)
BCREATE_R(OrExpr, Or, OrExpr_createPartial, OrExpr_createPartialR)
BCREATE_R(XorExpr, Xor, XorExpr_createPartial, XorExpr_createPartialR)
BCREATE(UDivExpr, UDiv)
BCREATE(SDivExpr, SDiv)
BCREATE(URemExpr, URem)
BCREATE(SRemExpr, SRem)
BCREATE(ShlExpr, Shl)
BCREATE(LShrExpr, LShr)
BCREATE(AShrExpr, AShr)

#define CMPCREATE(_e_op, _op) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r) { \
  assert(l->getWidth()==r->getWidth() && "type mismatch");              \
  if (ConstantExpr *cl = dyn_cast<ConstantExpr>(l))                     \
    if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r))                   \
      return cl->_op(cr);                                               \
  return _e_op ## _create(l, r);                                        \
}

#define CMPCREATE_T(_e_op, _op, _reflexive_e_op, partialL, partialR) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r) {    \
  assert(l->getWidth()==r->getWidth() && "type mismatch");             \
  if (ConstantExpr *cl = dyn_cast<ConstantExpr>(l)) {                  \
    if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r))                  \
      return cl->_op(cr);                                              \
    return partialR(cl, r.get());                                      \
  } else if (ConstantExpr *cr = dyn_cast<ConstantExpr>(r)) {           \
    return partialL(l.get(), cr);                                      \
  } else {                                                             \
    return _e_op ## _create(l.get(), r.get());                         \
  }                                                                    \
}


static ref<Expr> EqExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l == r) {
    return ConstantExpr::alloc(1, Expr::Bool);
  } else {
    return EqExpr::alloc(l, r);
  }
}


/// Tries to optimize EqExpr cl == rd, where cl is a ConstantExpr and
/// rd a ReadExpr.  If rd is a read into an all-constant array,
/// returns a disjunction of equalities on the index.  Otherwise,
/// returns the initial equality expression. 
static ref<Expr> TryConstArrayOpt(const ref<ConstantExpr> &cl,
				  ReadExpr *rd) {
  if (rd->updates.root->isSymbolicArray() || rd->updates.getSize())
    return EqExpr_create(cl, rd);

  // Number of positions in the array that contain value ct.
  unsigned numMatches = 0;

  // for now, just assume standard "flushing" of a concrete array,
  // where the concrete array has one update for each index, in order
  ref<Expr> res = ConstantExpr::alloc(0, Expr::Bool);
  for (unsigned i = 0, e = rd->updates.root->size; i != e; ++i) {
    if (cl == rd->updates.root->constantValues[i]) {
      // Arbitrary maximum on the size of disjunction.
      if (++numMatches > 100)
        return EqExpr_create(cl, rd);

      ref<Expr> mayBe =
        EqExpr::create(rd->index, ConstantExpr::alloc(i,
                                                      rd->index->getWidth()));
      res = OrExpr::create(res, mayBe);
    }
  }

  return res;
}

static ref<Expr> EqExpr_createPartialR(const ref<ConstantExpr> &cl, Expr *r) {
  Expr::Width width = cl->getWidth();

  Expr::Kind rk = r->getKind();
  if (width == Expr::Bool) {
    if (cl->isTrue()) {
      return r;
    } else {
      // 0 == ...

      if (rk == Expr::Eq) {
        const EqExpr *ree = cast<EqExpr>(r);

        // eliminate double negation
        if (ConstantExpr *CE = dyn_cast<ConstantExpr>(ree->left)) {
          // 0 == (0 == A) => A
          if (CE->getWidth() == Expr::Bool &&
              CE->isFalse())
            return ree->right;
        }
      } else if (rk == Expr::Or) {
        const OrExpr *roe = cast<OrExpr>(r);

        // transform not(or(a,b)) to and(not a, not b)
        return AndExpr::create(Expr::createIsZero(roe->left),
                               Expr::createIsZero(roe->right));
      }
    }
  } else if (rk == Expr::SExt) {
    // (sext(a,T)==c) == (a==c)
    const SExtExpr *see = cast<SExtExpr>(r);
    Expr::Width fromBits = see->src->getWidth();
    ref<ConstantExpr> trunc = cl->ZExt(fromBits);

    // pathological check, make sure it is possible to
    // sext to this value *from any value*
    if (cl == trunc->SExt(width)) {
      return EqExpr::create(see->src, trunc);
    } else {
      return ConstantExpr::create(0, Expr::Bool);
    }
  } else if (rk == Expr::ZExt) {
    // (zext(a,T)==c) == (a==c)
    const ZExtExpr *zee = cast<ZExtExpr>(r);
    Expr::Width fromBits = zee->src->getWidth();
    ref<ConstantExpr> trunc = cl->ZExt(fromBits);

    // pathological check, make sure it is possible to
    // zext to this value *from any value*
    if (cl == trunc->ZExt(width)) {
      return EqExpr::create(zee->src, trunc);
    } else {
      return ConstantExpr::create(0, Expr::Bool);
    }
  } else if (rk==Expr::Add) {
    const AddExpr *ae = cast<AddExpr>(r);
    if (isa<ConstantExpr>(ae->left)) {
      // c0 = c1 + b => c0 - c1 = b
      return EqExpr_createPartialR(cast<ConstantExpr>(SubExpr::create(cl,
                                                                      ae->left)),
                                   ae->right.get());
    }
  } else if (rk==Expr::Sub) {
    const SubExpr *se = cast<SubExpr>(r);
    if (isa<ConstantExpr>(se->left)) {
      // c0 = c1 - b => c1 - c0 = b
      return EqExpr_createPartialR(cast<ConstantExpr>(SubExpr::create(se->left,
                                                                      cl)),
                                   se->right.get());
    }
  } else if (rk == Expr::Read && ConstArrayOpt) {
    return TryConstArrayOpt(cl, static_cast<ReadExpr*>(r));
  }

  return EqExpr_create(cl, r);
}

static ref<Expr> EqExpr_createPartial(Expr *l, const ref<ConstantExpr> &cr) {
  return EqExpr_createPartialR(cr, l);
}




ref<Expr> NeExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return EqExpr::create(ConstantExpr::create(0, Expr::Bool),
                        EqExpr::create(l, r));
}

ref<Expr> UgtExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return UltExpr::create(r, l);
}
ref<Expr> UgeExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return UleExpr::create(r, l);
}

ref<Expr> SgtExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return SltExpr::create(r, l);
}
ref<Expr> SgeExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return SleExpr::create(r, l);
}

static ref<Expr> UltExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  Expr::Width t = l->getWidth();
  if (t == Expr::Bool) { // !l && r
    return AndExpr::create(Expr::createIsZero(l), r);
  } else {
    return UltExpr::alloc(l, r);
  }
}

static ref<Expr> UleExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // !(l && !r)
    return OrExpr::create(Expr::createIsZero(l), r);
  } else {
    return UleExpr::alloc(l, r);
  }
}

static ref<Expr> SltExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // l && !r
    return AndExpr::create(l, Expr::createIsZero(r));
  } else {
    return SltExpr::alloc(l, r);
  }
}

static ref<Expr> SleExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // !(!l && r)
    return OrExpr::create(l, Expr::createIsZero(r));
  } else {
    return SleExpr::alloc(l, r);
  }
}

CMPCREATE_T(EqExpr, Eq, EqExpr, EqExpr_createPartial, EqExpr_createPartialR)
CMPCREATE(UltExpr, Ult)
CMPCREATE(UleExpr, Ule)
CMPCREATE(SltExpr, Slt)
CMPCREATE(SleExpr, Sle)
