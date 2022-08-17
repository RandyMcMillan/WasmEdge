// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

#include "loader/loader.h"

namespace WasmEdge {
namespace Loader {

Expect<void> Loader::loadModule(std::unique_ptr<AST::Module> &Mod) {
  auto M = loadModule();
  Mod = std::move(M.value());
  return {};
}

Expect<void> Loader::loadSection(AST::ModuleSection &Sec) {
  return loadSectionContent(Sec, [this, &Sec]() {
    return loadSectionContentVec(
        Sec,
        [this](std::unique_ptr<AST::Module> &Mod) { return loadModule(Mod); });
  });
}

Expect<void> Loader::loadSection(AST::CoreInstanceSection &Sec) {
  return loadSectionContent(Sec, [this, &Sec]() {
    return loadSectionContentVec(Sec, [this](AST::CoreInstance &Instance) {
      return loadCoreInstance(Instance);
    });
  });
}
Expect<void> Loader::loadCoreInstance(AST::CoreInstance &Instance) {
  // core:instance ::= ie:<instance-expr> => (instance ie)
  auto Res = FMgr.readByte();
  if (!Res.has_value()) {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_CoreInstance);
  }

  // core:instanceexpr ::=
  switch (Res.value()) {
  case 0x00:
    // 0x00 m:<moduleidx> arg*:vec(<core:instantiatearg>)
    // => (instantiate m arg*)
    if (auto ModIdx = FMgr.readU32(); ModIdx.has_value()) {
      AST::CoreInstantiate Inst;
      Instance = Inst;

      Inst.setModuleIdx(ModIdx.value());
      return loadVec(Inst.getInstantiateArgs(),
                     [this](AST::CoreInstantiateArg &InstArg) -> Expect<void> {
                       auto Name = FMgr.readName();
                       if (!Name.has_value()) {
                         return logLoadError(Name.error(), FMgr.getLastOffset(),
                                             ASTNodeAttr::CompSec_CoreInstance);
                       }
                       auto B = FMgr.readByte();
                       if (!B.has_value()) {
                         return logLoadError(B.error(), FMgr.getLastOffset(),
                                             ASTNodeAttr::CompSec_CoreInstance);
                       }
                       auto Idx = FMgr.readU32();
                       if (!Idx.has_value()) {
                         return logLoadError(Idx.error(), FMgr.getLastOffset(),
                                             ASTNodeAttr::CompSec_CoreInstance);
                       }
                       InstArg.setName(Name.value());
                       InstArg.setIndex(Idx.value());

                       return {};
                     });
    } else {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_CoreInstance);
    }
  case 0x01:
    // 0x01 e*:vec(<core:export>) => e*
    {
      AST::CoreExportsInstance Inst;
      Instance = Inst;

      return loadVec(Inst.getExports(),
                     [this](AST::ExportDecl &Export) -> Expect<void> {
                       // core:export ::= n:<name> si:<core:sortidx>
                       // => (export n si)
                       if (auto Res = FMgr.readName(); Res.has_value()) {
                         Export.setName(*Res);
                       } else {
                         return logLoadError(Res.error(), FMgr.getLastOffset(),
                                             ASTNodeAttr::CompSec_Export);
                       }
                       return loadCoreSortIndex(Export.getExtern());
                     });
    }
  default:
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_CoreInstance);
  }
}

Expect<void> Loader::loadSection(AST::CoreAliasSection &Sec) {
  return loadSectionContent(Sec, [this, &Sec]() {
    return loadSectionContentVec(
        Sec, [this](AST::CoreAlias &Alias) { return loadCoreAlias(Alias); });
  });
}
Expect<void> Loader::loadCoreAlias(AST::CoreAlias &Alias) {
  // core:alias ::= sort:<core:sort> target:<core:aliastarget>
  //                => (core alias target (sort))
  if (auto Res = FMgr.readByte(); Res.has_value()) {
    Alias.setSort(Res.value());
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_CoreAlias);
  }
  // core:aliastarget ::=
  //     0x00 i:<core:instanceidx> n:<name> => export i n
  //   | 0x01 ct:<u32> idx:<u32>            => outer ct idx
  auto Res = FMgr.readByte();
  if (!Res) {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_CoreAlias);
  }
  switch (Res.value()) {
  case 0x00: {
    uint32_t Idx;
    std::string_view Name;
    if (auto IR = FMgr.readU32(); IR.has_value()) {
      Idx = IR.value();
    } else {
      return logLoadError(IR.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_CoreAlias);
    }
    if (auto NR = FMgr.readName(); NR.has_value()) {
      Name = NR.value();
    } else {
      return logLoadError(NR.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_CoreAlias);
    }
    Alias.setTarget(AST::CoreAliasTarget::Export(Idx, Name));
    break;
  }
  case 0x01: {
    uint32_t C, I;
    if (auto CR = FMgr.readU32(); CR.has_value()) {
      C = CR.value();
    } else {
      return logLoadError(CR.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_CoreAlias);
    }
    if (auto IR = FMgr.readU32(); IR.has_value()) {
      I = IR.value();
    } else {
      return logLoadError(IR.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_CoreAlias);
    }
    Alias.setTarget(AST::CoreAliasTarget::Outer(C, I));
    break;
  }
  default:
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_CoreAlias);
  }
  return {};
}

Expect<void> Loader::loadSection(AST::CoreTypeSection &Sec) {
  return loadSectionContent(Sec, [this, &Sec]() {
    return loadSectionContentVec(
        Sec, [this](AST::CoreType &Ty) { return loadCoreType(Ty); });
  });
}
Expect<void> Loader::loadCoreType(AST::CoreType &Ty) {
  // core:type ::= dt:<core:deftype> => (type dt) (GC proposal)
  // core:deftype ::= ft:<core:functype>   => ft (WebAssembly 1.0)
  //                | st:<core:structtype> => st (GC proposal)
  //                | at:<core:arraytype>  => at (GC proposal)
  //                | mt:<core:moduletype> => mt
  auto Res = FMgr.readByte();
  if (!Res.has_value()) {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_CoreType);
  }

  switch (Res.value()) {
  case 0x60U: {
    AST::CoreDefType::FuncType FT;
    Ty = FT;
    return noJudgeLoadType(FT);
  }
  case 0x21U: {
    // core:structtype ::= 0x21 ft*:vec(fieldtype)
    // field ::= t:storagetype mut:mutability
    AST::CoreDefType::StructType ST;
    Ty = ST;
    return loadVec(ST.getFieldTypes(),
                   [this](AST::FieldType &Ty) -> Expect<void> {
                     return loadFieldType(Ty);
                   });
  }
  case 0x22U: {
    // core:arraytype ::= 0x22 ft:fieldtype
    // field ::= t:storagetype mut:mutability
    AST::FieldType FT;
    Ty = AST::CoreDefType::ArrayType(FT);
    return loadFieldType(FT);
  }
  case 0x50U: {
    // core:moduletype ::= 0x50 md*:vec(<core:moduledecl>) => (module md*)
    AST::CoreDefType::ModuleType MT;
    Ty = MT;
    return loadVec(
        MT.getModuleDecls(), [this](AST::ModuleDecl &ModDecl) -> Expect<void> {
          // core:moduledecl ::= 0x00 i:<core:import>     => i
          //                   | 0x01 t:<core:type>       => t
          //                   | 0x02 a:<core:alias>      => a
          //                   | 0x03 e:<core:exportdecl> => e
          // core:exportdecl ::= n:<name> d:<core:importdesc> => (export n d)
          auto DeclType = FMgr.readByte();
          if (!DeclType.has_value()) {
            return logLoadError(DeclType.error(), FMgr.getLastOffset(),
                                ASTNodeAttr::CompSec_CoreType);
          }

          switch (DeclType.value()) {
          case 0x00: {
            // 0x00 i:<core:import>     => i
            AST::ImportDesc Desc;
            ModDecl = Desc;
            return loadDesc(Desc);
          }
          case 0x01: {
            // 0x01 t:<core:type>       => t
            AST::CoreType CT;
            ModDecl = CT;
            return loadCoreType(CT);
          }
          case 0x02: {
            // 0x02 a:<core:alias>      => a
            AST::CoreAlias Alias;
            ModDecl = Alias;
            return loadCoreAlias(Alias);
          }
          case 0x03: {
            // 0x03 e:<core:exportdecl> => e
            // core:exportdecl ::= n:<name> d:<core:importdesc> => (export n d)
            AST::ExportDesc Desc;
            ModDecl = Desc;
            return loadDesc(Desc);
          }
          default:
            return logLoadError(DeclType.error(), FMgr.getLastOffset(),
                                ASTNodeAttr::CompSec_CoreType);
          }
        });
  }
  default:
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_CoreType);
  }
}
Expect<void> Loader::loadFieldType(AST::FieldType &Ty) {
  auto StorageTy = FMgr.readByte();
  if (!StorageTy.has_value()) {
    return logLoadError(StorageTy.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_CoreType);
  }
  auto Mutability = FMgr.readByte();
  if (!Mutability.has_value()) {
    return logLoadError(Mutability.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_CoreType);
  }
  switch (StorageTy.value()) {
  case 0x06:
    Ty = AST::FieldType::I8(Mutability.value());
    break;
  case 0x07:
    Ty = AST::FieldType::I16(Mutability.value());
    break;
  }
  return {};
}

Expect<void> Loader::loadSection(AST::ComponentSection &Sec) {
  return loadSectionContent(Sec, [this, &Sec]() {
    return loadSectionContentVec(
        Sec, [this](std::unique_ptr<AST::Component> &Comp) -> Expect<void> {
          auto C = loadComponent();
          Comp = std::move(C.value());
          return {};
        });
  });
}

Expect<void> Loader::loadSection(AST::InstanceSection &Sec) {
  return loadSectionContent(Sec, [this, &Sec]() {
    return loadSectionContentVec(Sec,
                                 [this](AST::Instance &Inst) -> Expect<void> {
                                   return loadInstance(Inst);
                                 });
  });
}
Expect<void> Loader::loadInstance(AST::Instance &Inst) {
  // instance            ::= ie:<instance-expr> => (instance ie)
  auto Res = FMgr.readByte();
  if (!Res) {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Instance);
  }

  // instanceexpr       ::= 0x00 c:<componentidx> arg*:vec(<instantiatearg>)
  //                      | 0x01 e*:vec(<export>)
  // instantiatearg     ::= n:<name> si:<sortidx> => (with n si)
  switch (*Res) {
  case 0x00: {
    // 0x00 c:<componentidx> arg*:vec(<instantiatearg>) => (instantiate c arg*)
    AST::InstanceExpr::Instantiate Instantiate;
    Inst = Instantiate;

    auto Idx = FMgr.readU32();
    if (!Idx) {
      return logLoadError(Idx.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_Instance);
    }
    Instantiate.setIndex(*Idx);
    return loadVec(Instantiate.getArgs(),
                   [this](AST::InstantiateArg &Arg) -> Expect<void> {
                     auto Name = FMgr.readName();
                     if (!Name) {
                       return logLoadError(Name.error(), FMgr.getLastOffset(),
                                           ASTNodeAttr::CompSec_Instance);
                     }
                     Arg.setName(*Name);
                     return loadSortIndex(Arg.getSortIndex());
                   });
  }
  case 0x01: {
    // 0x01 e*:vec(<export>) => e*
    AST::InstanceExpr::Export Export;
    Inst = Export;

    return loadVec(Export.getExports(),
                   [this](AST::ExportDecl &ExpDecl) -> Expect<void> {
                     return loadExportDecl(ExpDecl);
                   });
  }
  default:
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Instance);
  }
}

Expect<void> Loader::loadSection(AST::AliasSection &Sec) {
  return loadSectionContent(Sec, [this, &Sec]() {
    return loadSectionContentVec(
        Sec,
        [this](AST::Alias &Alias) -> Expect<void> { return loadAlias(Alias); });
  });
}
Expect<void> Loader::loadAlias(AST::Alias &Alias) {
  // alias       ::= sort:<sort> target:<aliastarget> => (alias target (sort))
  loadSort(Alias.getSort());

  // aliastarget ::= 0x00 i:<instanceidx> n:<name>    => export i n
  //               | 0x01 ct:<u32> idx:<u32>          => outer ct idx
  auto Res = FMgr.readByte();
  if (!Res) {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Alias);
  }
  switch (*Res) {
  case 0x00: {
    // 0x00 i:<instanceidx> n:<name>    => export i n
    auto Idx = FMgr.readU32();
    if (!Idx) {
      return logLoadError(Idx.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_Alias);
    }
    auto Name = FMgr.readName();
    if (!Name) {
      return logLoadError(Name.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_Alias);
    }
    Alias.getTarget() = AST::AliasTarget::Export(*Idx, *Name);
    break;
  }
  case 0x01: {
    // 0x01 ct:<u32> idx:<u32>          => outer ct idx
    auto Ct = FMgr.readU32();
    if (!Ct) {
      return logLoadError(Ct.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_Alias);
    }
    auto Idx = FMgr.readU32();
    if (!Idx) {
      return logLoadError(Idx.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_Alias);
    }
    Alias.getTarget() = AST::AliasTarget::Outer(*Ct, *Idx);
    break;
  }
  default:
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Alias);
  }
  return {};
}

Expect<void> Loader::loadSection(AST::ComponentTypeSection &Sec) {
  return loadSectionContent(Sec, [this, &Sec]() {
    return loadSectionContentVec(
        Sec, [this](AST::Type &Ty) -> Expect<void> { return loadType(Ty); });
  });
}
Expect<void> Loader::loadType(AST::Type &Ty) {
  auto Res = FMgr.readByte();
  if (!Res) {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Type);
  }

  AST::DefinedValueType::Prim P;
  Ty = P;

  switch (*Res) {
  case 0x40:
    // => (func (param p)* (result r)*)
    // functype ::= 0x40 p*:<funcvec> r*:<funcvec>
    break;
  case 0x41:
    // componenttype ::= 0x41 cd*:vec(<componentdecl>) => (component cd*)
    break;
  case 0x42:
    // instancetype  ::= 0x42 id*:vec(<instancedecl>) => (instance id*)
    break;
  case static_cast<Byte>(AST::PrimitiveValueType::Bool):
    P.setValue(AST::PrimitiveValueType::Bool);
    return {};
  case static_cast<Byte>(AST::PrimitiveValueType::S8):
    P.setValue(AST::PrimitiveValueType::S8);
    return {};
  case static_cast<Byte>(AST::PrimitiveValueType::U8):
    P.setValue(AST::PrimitiveValueType::U8);
    return {};
  case static_cast<Byte>(AST::PrimitiveValueType::S16):
    P.setValue(AST::PrimitiveValueType::S16);
    return {};
  case static_cast<Byte>(AST::PrimitiveValueType::U16):
    P.setValue(AST::PrimitiveValueType::U16);
    return {};
  case static_cast<Byte>(AST::PrimitiveValueType::S32):
    P.setValue(AST::PrimitiveValueType::S32);
    return {};
  case static_cast<Byte>(AST::PrimitiveValueType::U32):
    P.setValue(AST::PrimitiveValueType::U32);
    return {};
  case static_cast<Byte>(AST::PrimitiveValueType::Float32):
    P.setValue(AST::PrimitiveValueType::Float32);
    return {};
  case static_cast<Byte>(AST::PrimitiveValueType::Float64):
    P.setValue(AST::PrimitiveValueType::Float64);
    return {};
  case static_cast<Byte>(AST::PrimitiveValueType::Char):
    P.setValue(AST::PrimitiveValueType::Char);
    return {};
  case static_cast<Byte>(AST::PrimitiveValueType::String):
    P.setValue(AST::PrimitiveValueType::String);
    return {};
  case 0x72:
  case 0x71:
  case 0x70:
  case 0x6f:
  case 0x6e:
  case 0x6d:
  case 0x6c:
  case 0x6b:
  case 0x6a:
  default:
    break;
  }
  return {};
}

Expect<void> Loader::loadSection(AST::ComponentCanonSection &Sec) {
  return loadSectionContent(Sec, [this, &Sec]() {
    return loadSectionContentVec(
        Sec, [this](AST::Canon &Canon) { return loadCanon(Canon); });
  });
}
Expect<void> Loader::loadCanon(AST::Canon &Canon) {
  if (auto DirectionByte = FMgr.readByte(); DirectionByte.has_value()) {
    switch (DirectionByte.value()) {
    case 0x00: {
      // 0x00 0x00 f:<core:funcidx> opts:<opts> ft:<typeidx>
      // => (canon lift f opts type-index-space[ft])
      AST::Canon::Lift CanonLift;
      Canon = CanonLift;
      if (auto Res = FMgr.readByte(); !Res.has_value() || Res.value() != 0x00) {
        return logLoadError(Res.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Canon);
      }
      // f:<core:funcidx>
      auto CoreFuncIdx = FMgr.readU32();
      if (!CoreFuncIdx.has_value()) {
        return logLoadError(CoreFuncIdx.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Canon);
      }
      CanonLift.setCoreFuncIdx(CoreFuncIdx.value());

      // opts ::= opt*:vec(<canonopt>) => opt*
      auto Res = loadVec(CanonLift.getOpts(),
                         [this](AST::CanonOpt &CanonOpt) -> Expect<void> {
                           return loadCanonOpt(CanonOpt);
                         });
      if (!Res.has_value()) {
        return Res;
      }

      // ft:<typeidx>
      auto TypeIdx = FMgr.readByte();
      if (!TypeIdx.has_value()) {
        return logLoadError(TypeIdx.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Canon);
      }
      CanonLift.setTypeIdx(TypeIdx.value());
      break;
    }
    case 0x01: {
      // 0x01 0x00 f:<funcidx> opts:<opts>
      // => (canon lower f opts (core func))
      AST::Canon::Lower CanonLower;
      Canon = CanonLower;
      if (auto Res = FMgr.readByte(); !Res.has_value() || Res.value() != 0x00) {
        return logLoadError(Res.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Canon);
      }
      auto FuncIdx = FMgr.readU32();
      if (!FuncIdx.has_value()) {
        return logLoadError(FuncIdx.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Canon);
      }
      CanonLower.setFuncIdx(FuncIdx.value());

      // opts ::= opt*:vec(<canonopt>) => opt*
      return loadVec(CanonLower.getOpts(),
                     [this](AST::CanonOpt &CanonOpt) -> Expect<void> {
                       return loadCanonOpt(CanonOpt);
                     });
    }
    default:
      return logLoadError(DirectionByte.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_Canon);
    }

    return {};
  } else {
    return logLoadError(DirectionByte.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Canon);
  }
}
Expect<void> Loader::loadCanonOpt(AST::CanonOpt &CanonOpt) {
  auto CanonOptType = FMgr.readByte();
  if (!CanonOptType.has_value()) {
    return logLoadError(CanonOptType.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Canon);
  }
  switch (CanonOptType.value()) {
  case 0x00: {
    // 0x00 => string-encoding=utf8
    CanonOpt = AST::CanonOpt::StringEncodingUTF8();
    break;
  }
  case 0x01: {
    // 0x01 => string-encoding=utf16
    CanonOpt = AST::CanonOpt::StringEncodingUTF16();
    break;
  }
  case 0x02: {
    // 0x02 => string-encoding=latin1+utf16
    CanonOpt = AST::CanonOpt::StringEncodingLatin1UTF16();
    break;
  }
  case 0x03: {
    // 0x03 m:<core:memidx> => (memory m)
    auto Res = FMgr.readU32();
    if (!Res.has_value()) {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_Canon);
    }
    CanonOpt = AST::CanonOpt::MemoryIndex(Res.value());
    break;
  }
  case 0x04: {
    // 0x04 f:<core:funcidx> => (realloc f)
    auto Res = FMgr.readU32();
    if (!Res.has_value()) {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_Canon);
    }
    CanonOpt = AST::CanonOpt::ReallocFunc(Res.value());
    break;
  }
  case 0x05: {
    // 0x05 f:<core:funcidx> => (post-return f)
    auto Res = FMgr.readU32();
    if (!Res.has_value()) {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_Canon);
    }
    CanonOpt = AST::CanonOpt::PostReturnFunc(Res.value());
    break;
  }
  default:
    return logLoadError(CanonOptType.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Canon);
  }
  return {};
}

Expect<void> Loader::loadSection(AST::ComponentStartSection &Sec) {
  // start ::= f:<funcidx> arg*:vec(<valueidx>)
  return loadSectionContent(Sec, [this, &Sec]() -> Expect<void> {
    auto FuncIdx = FMgr.readU32();
    if (!FuncIdx.has_value()) {
      return logLoadError(FuncIdx.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::CompSec_Start);
    }
    Sec.setFuncIdx(FuncIdx.value());
    return loadVec(Sec.getContent(),
                   [this](AST::StartValueIdx &ValueIdx) -> Expect<void> {
                     auto Idx = FMgr.readU32();
                     if (!Idx.has_value()) {
                       return logLoadError(Idx.error(), FMgr.getLastOffset(),
                                           ASTNodeAttr::CompSec_Start);
                     }
                     ValueIdx.setValueIdx(Idx.value());
                     return {};
                   });
  });
}

Expect<void> Loader::loadSection(AST::ComponentImportSection &Sec) {
  return loadSectionContent(Sec, [this, &Sec]() {
    return loadSectionContentVec(Sec, [this](AST::ImportDecl &Import) {
      return loadImportDecl(Import);
    });
  });
}
Expect<void> Loader::loadImportDecl(AST::ImportDecl &Import) {
  if (auto Res = FMgr.readName(); Res.has_value()) {
    Import.setName(*Res);
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Import);
  }

  Byte DescType;
  if (auto Res = FMgr.readByte(); Res.has_value()) {
    DescType = Res.value();
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Import);
  }

  switch (DescType) {
  case 0x00:
    // 0x00 0x11 i:<core:typeidx>           => (core module (type i))
    {
      if (auto Res = FMgr.readByte(); !Res.has_value() || Res.value() != 0x11) {
        return logLoadError(Res.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Import);
      }
      auto TypeIdx = FMgr.readU32();
      if (!TypeIdx.has_value()) {
        return logLoadError(TypeIdx.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Import);
      }
      Import.setExtern(AST::ExternDesc::CoreType(TypeIdx.value()));
      break;
    }
  case 0x01:
    // 0x01 i:<typeidx>                     => (func (type i))
    {
      auto TypeIdx = FMgr.readU32();
      if (!TypeIdx.has_value()) {
        return logLoadError(TypeIdx.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Import);
      }
      Import.setExtern(AST::ExternDesc::FuncType(TypeIdx.value()));
      break;
    }
  case 0x02:
    // 0x02 t:<valtype>                     => (value t)
    {
      uint32_t TypeIdx;
      if (auto Res = FMgr.readU32(); Res.has_value()) {
        TypeIdx = Res.value();
      } else {
        return logLoadError(Res.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Import);
      }
      AST::ExternDesc::ValueType Ty;
      switch (TypeIdx) {
      case 0x7f:
        Ty = AST::ExternDesc::ValueType::Bool();
      case 0x7e:
        Ty = AST::ExternDesc::ValueType::S8();
      case 0x7d:
        Ty = AST::ExternDesc::ValueType::U8();
      case 0x7c:
        Ty = AST::ExternDesc::ValueType::S16();
      case 0x7b:
        Ty = AST::ExternDesc::ValueType::U16();
      case 0x7a:
        Ty = AST::ExternDesc::ValueType::S32();
      case 0x79:
        Ty = AST::ExternDesc::ValueType::U32();
      case 0x78:
        Ty = AST::ExternDesc::ValueType::S64();
      case 0x77:
        Ty = AST::ExternDesc::ValueType::U64();
      case 0x76:
        Ty = AST::ExternDesc::ValueType::Float32();
      case 0x75:
        Ty = AST::ExternDesc::ValueType::Float64();
      case 0x74:
        Ty = AST::ExternDesc::ValueType::Char();
      case 0x73:
        Ty = AST::ExternDesc::ValueType::String();
      default:
        Ty = AST::ExternDesc::ValueType::Idx(TypeIdx);
      }
      Import.setExtern(Ty);
      break;
    }
  case 0x03:
    // 0x03 b:<typebound>                   => (type b)
    {
      if (auto Res = FMgr.readByte(); !Res.has_value() || Res.value() != 0x00) {
        return logLoadError(Res.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Import);
      }
      auto TypeIdx = FMgr.readU32();
      if (!TypeIdx.has_value()) {
        return logLoadError(TypeIdx.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Import);
      }
      Import.setExtern(AST::ExternDesc::TypeBound(TypeIdx.value()));
      break;
    }
  case 0x04:
    // 0x04 i:<typeidx>                     => (instance (type i))
    {
      auto TypeIdx = FMgr.readU32();
      if (!TypeIdx.has_value()) {
        return logLoadError(TypeIdx.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Import);
      }
      Import.setExtern(AST::ExternDesc::InstanceType(TypeIdx.value()));
      break;
    }
  case 0x05:
    // 0x05 i:<typeidx>                     => (component (type i))
    {
      auto TypeIdx = FMgr.readU32();
      if (!TypeIdx.has_value()) {
        return logLoadError(TypeIdx.error(), FMgr.getLastOffset(),
                            ASTNodeAttr::CompSec_Import);
      }
      Import.setExtern(AST::ExternDesc::ComponentType(TypeIdx.value()));
      break;
    }
  }

  return {};
}

Expect<void> Loader::loadSection(AST::ComponentExportSection &Sec) {
  return loadSectionContent(Sec, [this, &Sec]() {
    return loadSectionContentVec(Sec, [this](AST::ExportDecl &Export) {
      return loadExportDecl(Export);
    });
  });
}
Expect<void> Loader::loadExportDecl(AST::ExportDecl &Export) {
  // export ::= n:<name> si:<sortidx>
  // n:<name>
  if (auto Res = FMgr.readName(); Res.has_value()) {
    Export.setName(*Res);
    return loadSortIndex(Export.getExtern());
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Export);
  }
}

Expect<void> Loader::loadSortIndex(AST::SortIndex &SortIdx) {
  // sortidx             ::= sort:<sort> idx:<u32> => (sort idx)
  if (auto R = loadSort(SortIdx.getSort()); !R) {
    return logLoadError(R.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Export);
  }
  auto Idx = FMgr.readU32();
  if (!Idx) {
    return logLoadError(Idx.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Export);
  }
  SortIdx.setIndex(*Idx);
  return {};
}

Expect<void> Loader::loadCoreSortIndex(AST::SortIndex &SortIdx) {
  // core:sortidx ::= sort:<core:sort> idx:<u32> => (sort idx)
  if (auto R = loadCoreSort(SortIdx.getSort()); !R) {
    return logLoadError(R.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_CoreInstance);
  }
  auto Idx = FMgr.readU32();
  if (!Idx) {
    return logLoadError(Idx.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_CoreInstance);
  }
  SortIdx.setIndex(*Idx);
  return {};
}

Expect<void> Loader::loadSort(AST::Sort &Sort) {
  // sort                ::= 0x00 cs:<core:sort> => core cs
  //                       | 0x01 => func
  //                       | 0x02 => value
  //                       | 0x03 => type
  //                       | 0x04 => component
  //                       | 0x05 => instance
  // si:<sortidx>
  auto Res = FMgr.readByte();
  if (!Res) {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Alias);
  }
  switch (*Res) {
  case 0x00:
    return loadCoreSort(Sort);
  case 0x01:
    Sort = AST::Sort::Func;
    return {};
  case 0x02:
    Sort = AST::Sort::Value;
    return {};
  case 0x03:
    Sort = AST::Sort::Type;
    return {};
  case 0x04:
    Sort = AST::Sort::Component;
    return {};
  case 0x05:
    Sort = AST::Sort::Instance;
    return {};
  default:
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Alias);
  }
}
Expect<void> Loader::loadCoreSort(AST::Sort &Sort) {
  // core:sort ::= 0x00 => func
  //             | 0x01 => table
  //             | 0x02 => memory
  //             | 0x03 => global
  //             | 0x10 => type
  //             | 0x11 => module
  //             | 0x12 => instance
  auto Res = FMgr.readByte();
  if (!Res) {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Alias);
  }
  switch (*Res) {
  case 0x00:
    Sort = AST::Sort::CoreFunc;
    return {};
  case 0x01:
    Sort = AST::Sort::Table;
    return {};
  case 0x02:
    Sort = AST::Sort::Memory;
    return {};
  case 0x03:
    Sort = AST::Sort::Global;
    return {};
  case 0x10:
    Sort = AST::Sort::CoreType;
    return {};
  case 0x11:
    Sort = AST::Sort::Module;
    return {};
  case 0x12:
    Sort = AST::Sort::CoreInstance;
    return {};
  default:
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::CompSec_Alias);
  }
}

} // namespace Loader
} // namespace WasmEdge