// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2022 Second State INC

//=== wasmedge/ast/component/type_section.h - Type Section class definitions
//
// Part of the WasmEdge Project.
//
//===------------------------------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the Type node classes.
///
//===------------------------------------------------------------------------------------------===//
#pragma once

#include "ast/component/alias_section.h"
#include "ast/component/coretype_section.h"
#include "ast/component/export_section.h"
#include "ast/component/import_section.h"

#include <optional>
#include <string>
#include <vector>

namespace WasmEdge {
namespace AST {

class Type {};
class DefinedType : public Type {
public:
};

class DefinedValueType : public DefinedType {
public:
  class Prim;
  class Record;
  class Variant;
  class List;
  class Tuple;
  class Flags;
  class Enum;
  class Union;
  class Option;
  class Result;
};

enum class PrimitiveValueType : Byte {
  String = 0x73,
  Char = 0x74,
  Float64 = 0x75,
  Float32 = 0x76,
  U64 = 0x77,
  S64 = 0x78,
  U32 = 0x79,
  S32 = 0x7a,
  U16 = 0x7b,
  S16 = 0x7c,
  U8 = 0x7d,
  S8 = 0x7e,
  Bool = 0x7f
};

typedef union {
  uint32_t TypeIdx;
  PrimitiveValueType PrimValTy;
} ValueType;

class NamedValType {
  std::string Name;
  ValueType Ty;
};

typedef union {
  ValueType ValTy;
  std::vector<NamedValType> NamedValTypes;
} FuncVec;

class CaseType {
  std::optional<ValueType> Ty{std::nullopt};
};
class Case {
  // (case n t?)
  std::string Name;
  std::optional<CaseType> OptTy{std::nullopt};
  // (case n t? (refines case-label[i]))
  uint32_t LabelIdx;
};

class DefinedValueType::Prim : public DefinedValueType {
public:
  void setValue(PrimitiveValueType V) noexcept { Value = V; }
  PrimitiveValueType getValue() const noexcept { return Value; }

private:
  PrimitiveValueType Value;
};
class DefinedValueType::Record : public DefinedValueType {
  std::vector<NamedValType> Fields;
};
class DefinedValueType::Variant : public DefinedValueType {
  std::vector<Case> Cases;
};
class DefinedValueType::List : public DefinedValueType {
  ValueType Ty;
};
class DefinedValueType::Tuple : public DefinedValueType {
  std::vector<ValueType> Types;
};
class DefinedValueType::Flags : public DefinedValueType {
  std::vector<std::string> Names;
};
class DefinedValueType::Enum : public DefinedValueType {
  std::vector<std::string> Names;
};
class DefinedValueType::Union : public DefinedValueType {
  std::vector<ValueType> Types;
};
class DefinedValueType::Option : public DefinedValueType {
  ValueType Ty;
};
class DefinedValueType::Result : public DefinedValueType {
  std::optional<CaseType> ResultTy{std::nullopt};
  std::optional<CaseType> ErrorTy{std::nullopt};
};

class FuncType : public DefinedType {
  std::vector<FuncVec> Parameters;
  std::vector<FuncVec> Returns;
};

typedef union {
  // 0x00 t:<core:type>                   => t
  CoreType CoreTy;
  // 0x01 t:<type>                        => t
  Type Ty;
  // 0x02 a:<alias>                       => a
  Alias Alias;
  // 0x04 ed:<exportdecl>                 => ed
  ExportDecl Export;
} InstanceDecl;
class InstanceType : public DefinedType {
  std::vector<InstanceDecl> Decls;
};

typedef union {
  ImportDecl Import;
  InstanceDecl Instance;
} ComponentDecl;
class ComponentType : public DefinedType {
  std::vector<ComponentDecl> Decls;
};

} // namespace AST
} // namespace WasmEdge