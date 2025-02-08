#include <pqxx/internal/callgate.hxx>

namespace pqxx
{
class connection;
}

namespace pqxx::internal::gate
{
class PQXX_PRIVATE connection_transaction : callgate<connection>
{
  friend class pqxx::transaction_base;

  connection_transaction(reference x) : super(x) {}

  template<typename STRING>
  result exec(STRING query, std::string_view desc, PQXX_LOC loc)
  {
    return home().exec(query, desc, loc);
  }

  template<typename STRING> result exec(STRING query, PQXX_LOC loc)
  {
    return home().exec(query, "", loc);
  }

  void register_transaction(transaction_base *t)
  {
    home().register_transaction(t);
  }
  void unregister_transaction(transaction_base *t) noexcept
  {
    home().unregister_transaction(t);
  }

  auto read_copy_line() { return home().read_copy_line(); }
  void write_copy_line(std::string_view line, PQXX_LOC loc)
  {
    home().write_copy_line(line, loc);
  }
  void end_copy_write() { home().end_copy_write(); }

  result exec_prepared(
    std::string_view statement, internal::c_params const &args, PQXX_LOC loc)
  {
    return home().exec_prepared(statement, args, loc);
  }

  result exec_params(
    std::string_view query, internal::c_params const &args, PQXX_LOC loc)
  {
    return home().exec_params(query, args, loc);
  }
};
} // namespace pqxx::internal::gate
