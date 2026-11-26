#include "ors/ctp/QueryCadence.h"

namespace ors::ctp {

QueryKind nextQuery(QueryKind current) {
  return current == QueryKind::Account ? QueryKind::Position : QueryKind::Account;
}

}  // namespace ors::ctp
