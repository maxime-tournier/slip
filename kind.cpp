#include "kind.hpp"


namespace kind {

// constants
const ref<constant> term = make_ref<constant>(constant{symbol{"*"}});
const ref<constant> row = make_ref<constant>(constant{symbol{"row"}});

any operator>>=(any from, any to) {
  return make_ref<constructor>(constructor{from, to});
}

}
