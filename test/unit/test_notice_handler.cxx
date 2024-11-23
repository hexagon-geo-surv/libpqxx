#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
void test_notice_handler_receives_notice()
{
  pqxx::connection cx;
  pqxx::work tx{cx};

  // C++20: Use format library.
  tx.exec(R"(
    CREATE PROCEDURE pg_temp.say()
    LANGUAGE plpgsql
    AS
    $$
      BEGIN
        RAISE EXCEPTION 'Test notice';
      END;
    $$
    )").no_rows();

  int notices{0};
  std::string received;

  cx.set_notice_handler(
    [&notices, &received](pqxx::zview msg) noexcept
    {
      ++notices;
      received = msg;
    }
  );

  // Trigger a notice.
  PQXX_CHECK_THROWS_EXCEPTION(
    tx.exec("CALL pg_temp.say()").no_rows(),
    "Did not trigger expected exception.");

  PQXX_CHECK_EQUAL(notices, 1, "Did not get expected single notice.");
  PQXX_CHECK_EQUAL(received, "Test notice", "Wrong notice message.");
}


void test_notice_handler_works_after_connection_closes()
{
  pqxx::result r;
  int notices{0};

  {
    pqxx::connection cx;
    cx.set_notice_handler([&notices](pqxx::zview) noexcept { ++notices; });
    pqxx::work tx{cx};
    r = tx.exec("SELECT 1");
  }

  PQXX_CHECK_EQUAL(notices, 0, "Got premature notice.");

  // Trigger a notice.
  PQXX_CHECK_THROWS_EXCEPTION(
    pqxx::ignore_unused(r[0][99]),
    "Did not trigger expected exception.");

  PQXX_CHECK_EQUAL(
    notices, 1, "Did not get expected single post-connection notice.");
}


void test_process_notice_calls_notice_handler()
{
  int calls{0};
  std::string received;
  const std::string msg{"Hello there\n"};

  pqxx::connection cx;
  cx.set_notice_handler([&calls, &received](auto x){ ++calls; received = x; });
  cx.process_notice(msg);

  PQXX_CHECK_EQUAL(calls, 1, "Expected exactly 1 call to notice handler.");
  PQXX_CHECK_EQUAL(received, msg, "Got wrong message.");
}


// XXX: Test stateless lambda.
// XXX: Test function.
// XXX: Test functor by value.
// XXX: Test functor by reference.
// XXX: Test functor by rvalue reference.
// XXX: Test processing after copy/move construction/assignment.


PQXX_REGISTER_TEST(test_notice_handler_receives_notice);
PQXX_REGISTER_TEST(test_notice_handler_works_after_connection_closes);
PQXX_REGISTER_TEST(test_process_notice_calls_notice_handler);
} // namespace
