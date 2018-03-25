// This file is part of asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#include "test_init.hpp"
#include "../src/misc.hpp"

using namespace Asteria;

int main(){
	try {
		ASTERIA_THROW("test", ' ', "exception: ", 42);
		ASTERIA_TEST_CHECK(false);
	} catch(std::runtime_error &e){
		ASTERIA_TEST_CHECK(std::strstr(e.what(), "test exception: 42") != nullptr);
	}
}