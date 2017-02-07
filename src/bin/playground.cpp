#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <utility>

#include "operators/abstract_operator.hpp"
#include "operators/get_table.hpp"
#include "operators/nested_loop_join.hpp"
#include "operators/print.hpp"
#include "operators/sort_merge_join.hpp"
#include "operators/table_scan.hpp"
#include "storage/storage_manager.hpp"
#include "storage/table.hpp"

std::random_device rd;
std::mt19937 eng(rd());
std::uniform_int_distribution<> distr(0, 999);

int random_int() { return distr(eng); }

double random_double() {
  int a = distr(eng);
  int b = distr(eng);
  return static_cast<double>(a) + static_cast<double>(b) / 100.0;
}

float random_float() { return static_cast<float>(random_double()); }

template <typename JoinType>
int join_performance_test(int number_of_rows, int distinct_values, int chunk_size) {
  distr = std::uniform_int_distribution<>(0, distinct_values - 1);

  auto t1 = std::make_shared<opossum::Table>(opossum::Table(chunk_size));
  auto t2 = std::make_shared<opossum::Table>(opossum::Table(chunk_size));

  t1->add_column("a", "int");
  t1->add_column("b", "float");
  t1->add_column("d", "double");

  t2->add_column("a", "int");
  t2->add_column("b", "float");
  t2->add_column("d", "double");

  for (int i = 0; i < number_of_rows; i++) {
    t1->append({random_int(), random_float(), random_double()});
    t2->append({random_int(), random_float(), random_double()});
  }

  opossum::StorageManager::get().add_table("table1", std::move(t1));
  opossum::StorageManager::get().add_table("table2", std::move(t2));

  auto gt1 = std::make_shared<opossum::GetTable>("table1");
  gt1->execute();

  auto gt2 = std::make_shared<opossum::GetTable>("table2");
  gt2->execute();

  auto s = std::make_shared<JoinType>(gt1, gt2, std::pair<std::string, std::string>("a", "a"), "=",
                                      opossum::JoinMode::Inner);
  auto start = std::chrono::steady_clock::now();
  s->execute();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

  opossum::StorageManager::get().reset();

  // std::cout << s->get_output()->row_count() << std::endl;
  return duration.count();
}

int main() {
  for (int n = 10000; n < 1000000; n += 10000) {
    int duration = join_performance_test<opossum::NestedLoopJoin>(n, 1000, 1000);
    std::cout << n << ", " << duration << "" << std::endl;
  }
}
