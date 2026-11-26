#pragma once

namespace ors::ctp {

enum class QueryKind {
  Account,
  Position,
};

/// One query per round: account, position, account, ...
QueryKind nextQuery(QueryKind current);

}  // namespace ors::ctp
