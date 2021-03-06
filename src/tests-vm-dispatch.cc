// TODO: some header

#include "../3rdparty/catch.hpp"

#include <vata2/vm-dispatch.hh>
#include <vata2/vm.hh>

using namespace Vata2::VM;

TEST_CASE("Vata2::VM::find_dispatcher()")
{
	SECTION("invalid type")
	{
		CHECK_THROWS_WITH(find_dispatcher("UNKNOWN"),
			Catch::Contains("cannot find the dispatcher"));
	}

	SECTION("valid type")
	{
		size_t n42 = 42;
		auto f = [&n42](const VMFuncName&, const VMFuncArgs&) -> VMValue {
			return VMValue("ANSWER", &n42); };

		reg_dispatcher("FOO", f, "a foo data type");

		VMValue val = find_dispatcher("FOO")("BAR", { });
		REQUIRE(val.type == "ANSWER");
		REQUIRE(val.get_ptr() == &n42);
	}

	SECTION("trying to re-register a dispatcher")
	{
		CHECK_THROWS_WITH(reg_dispatcher(Vata2::TYPE_STR, nullptr, "a string data type"),
			Catch::Contains("already registered"));
	}
}
