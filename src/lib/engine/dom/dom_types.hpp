#pragma once

namespace up {

enum class DomNodeType { Global, Package, Target };
enum class VarValueType { Scalar, Script };
enum class ScriptMessageType {
  Unknown = 0,
  SourcesPreprocess,
  SourcesPostprocess,
  HeadersPreprocess,
  HeadersPostprocess,
  AssetsPreprocess,
  AssetsPostprocess,
  Manual,
};

}  // namespace up
