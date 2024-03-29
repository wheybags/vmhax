#include "test.h"
#include "../pinned.h"

class test_content
{
public:
  static int32_t live_count;

  test_content() { live_count++; }
  explicit test_content(int32_t val) : test_content() { this->val = val; }
  test_content(const test_content& other) : test_content() { this->val = other.val; }
  test_content(test_content&& other)  noexcept : test_content() { this->val = other.val; other.val = 0; }

  ~test_content()
  {
    val = 0xFFFFFFFF;
    live_count--;
  }

  test_content& operator=(const test_content& other) { val = other.val; return *this; }
  test_content& operator=(test_content&& other) noexcept
  {
    if (this != &other)
    {
      val = other.val;
      other.val = 0;
    }
    return *this;
  }

  int32_t val = 0xDEADBEEF;
};

int32_t test_content::live_count = 0;

#define vec_t pinned_vec
//#include <vector>
//#define vec_t std::vector

void test_vec_basic()
{
  {
    vec_t<test_content> vec(100);
    CHECK(test_content::live_count == 100);
    CHECK(vec.size() == 100);
    CHECK(vec.capacity() >= 100);
    CHECK(!vec.empty());

    for (int32_t i = 0; i < vec.size(); i++)
      CHECK(vec.at(i).val == 0xDEADBEEF);
  }
  CHECK(test_content::live_count == 0);
}

void test_vec_empty()
{
  {
    vec_t<test_content> vec;
    CHECK(test_content::live_count == 0);
    CHECK(vec.size() == 0);
    CHECK(vec.empty());
    CHECK(vec.capacity() == 0);

    for (int32_t i = 0; i < 512; i++)
    {
      vec.resize(i+1, test_content(i));
      CHECK(test_content::live_count == i + 1);
      CHECK(vec.size() == i+1);
      CHECK(!vec.empty());
      CHECK(vec.capacity() >= i+1);
    }

    for (int32_t i = 0; i < 512; i++)
      CHECK(vec.at(i).val == i);
  }
  CHECK(test_content::live_count == 0);
}

void test_vec_push_pop()
{
  vec_t<test_content> vec;
  CHECK(test_content::live_count == 0);

  vec.push_back(test_content(0));
  vec.push_back(test_content(1));
  vec.push_back(test_content(2));
  vec.push_back(test_content(3));

  CHECK(test_content::live_count == 4);
  CHECK(vec.size() == 4);

  CHECK(vec[0].val == 0);
  CHECK(vec[1].val == 1);
  CHECK(vec[2].val == 2);
  CHECK(vec[3].val == 3);

  CHECK(vec.front().val == 0);
  CHECK(vec.back().val == 3);
  vec.pop_back();

  CHECK(test_content::live_count == 3);
  CHECK(vec.size() == 3);
  CHECK(vec.back().val == 2);
  CHECK(vec.front().val == 0);

  vec.pop_back();
  vec.pop_back();
  vec.pop_back();

  CHECK(test_content::live_count == 0);
  CHECK(vec.empty());
}

void test_vec_clear()
{
  vec_t<test_content> vec;
  vec.resize(100);

  CHECK(test_content::live_count == 100);
  CHECK(vec.size() == 100);

  vec.clear();

  CHECK(test_content::live_count == 0);
  CHECK(vec.empty());

  vec.clear();

  CHECK(test_content::live_count == 0);
  CHECK(vec.empty());

  vec.resize(100);

  CHECK(test_content::live_count == 100);
  CHECK(vec.size() == 100);

  vec.clear();

  CHECK(test_content::live_count == 0);
  CHECK(vec.empty());
}

void test_vec_realloc()
{
  vec_t<test_content> vec(1, test_content(0));

  size_t old_capacity = vec.capacity();

  int32_t size = 1;
  while (vec.capacity() == old_capacity)
  {
    vec.push_back(test_content(size));
    size++;
  }

  for (int32_t i = 0; i < 10; i++)
  {
    vec.push_back(test_content(size));
    size++;
  }

  CHECK(test_content::live_count == size);
  CHECK(vec.size() == size);

  for (int32_t i = 0; i < size; i++)
    CHECK(vec[i].val == i);
}

void test_vec_shrink_to_fit()
{
  vec_t<test_content> vec(1);

  size_t original_capacity = vec.capacity();
  while (vec.capacity() == original_capacity)
    vec.emplace_back();

  for (int32_t i = 0; i < 10; i++)
    vec.emplace_back();

  size_t expanded_capacity = vec.capacity();
  vec.resize(original_capacity);

  CHECK(vec.capacity() == expanded_capacity);

  vec.shrink_to_fit();
  CHECK(vec.capacity() == original_capacity);
}

void test_vec_insert_begin()
{
  {
    vec_t<test_content> vec;

    auto it = vec.insert(vec.begin(), test_content(0));
    CHECK(it->val == 0);
    it = vec.insert(vec.begin(), test_content(1));
    CHECK(it->val == 1);
    vec.insert(vec.begin(), test_content(2));
    vec.insert(vec.begin(), test_content(3));

    CHECK(vec.size() == 4);
    CHECK(test_content::live_count == 4);

    CHECK(vec[0].val == 3);
    CHECK(vec[1].val == 2);
    CHECK(vec[2].val == 1);
    CHECK(vec[3].val == 0);
  }

  CHECK(test_content::live_count == 0);
}

void test_vec_insert_middle()
{
  {
    vec_t<test_content> dest_vec;

    for (int32_t i = 0; i < 11; i++)
      dest_vec.emplace_back(i);

    CHECK(dest_vec.size() == 11);

    {
      vec_t<test_content> source_vec;
      source_vec.emplace_back(21);
      source_vec.emplace_back(22);
      source_vec.emplace_back(23);

      auto it = dest_vec.insert(dest_vec.begin() + 4, source_vec.begin(), source_vec.end());
      CHECK(it->val = 21);
    }

    CHECK(dest_vec.size() == 14);
    CHECK(test_content::live_count == 14);

    CHECK(dest_vec[0].val == 0);
    CHECK(dest_vec[1].val == 1);
    CHECK(dest_vec[2].val == 2);
    CHECK(dest_vec[3].val == 3);
    CHECK(dest_vec[4].val == 21);
    CHECK(dest_vec[5].val == 22);
    CHECK(dest_vec[6].val == 23);
    CHECK(dest_vec[7].val == 4);
    CHECK(dest_vec[8].val == 5);
    CHECK(dest_vec[9].val == 6);
    CHECK(dest_vec[10].val == 7);
    CHECK(dest_vec[11].val == 8);
    CHECK(dest_vec[12].val == 9);
    CHECK(dest_vec[13].val == 10);
  }

  CHECK(test_content::live_count == 0);
}

void test_vec_move_insert_range()
{
  {
    vec_t<test_content> dest_vec;

    for (int32_t i = 0; i < 11; i++)
      dest_vec.emplace_back(i);

    CHECK(dest_vec.size() == 11);

    {
      vec_t<test_content> source_vec;
      source_vec.emplace_back(21);
      source_vec.emplace_back(22);
      source_vec.emplace_back(23);

      auto it = dest_vec.insert(dest_vec.begin() + 4, std::make_move_iterator(source_vec.begin()), std::make_move_iterator(source_vec.end()));
      CHECK(it->val = 21);

      CHECK(source_vec[0].val == 0);
      CHECK(source_vec[1].val == 0);
      CHECK(source_vec[2].val == 0);
    }

    CHECK(dest_vec.size() == 14);
    CHECK(test_content::live_count == 14);

    CHECK(dest_vec[0].val == 0);
    CHECK(dest_vec[1].val == 1);
    CHECK(dest_vec[2].val == 2);
    CHECK(dest_vec[3].val == 3);
    CHECK(dest_vec[4].val == 21);
    CHECK(dest_vec[5].val == 22);
    CHECK(dest_vec[6].val == 23);
    CHECK(dest_vec[7].val == 4);
    CHECK(dest_vec[8].val == 5);
    CHECK(dest_vec[9].val == 6);
    CHECK(dest_vec[10].val == 7);
    CHECK(dest_vec[11].val == 8);
    CHECK(dest_vec[12].val == 9);
    CHECK(dest_vec[13].val == 10);
  }

  CHECK(test_content::live_count == 0);
}

void test_vec_erase_simple()
{
  vec_t<test_content> vec;

  for (int32_t i = 0; i < 10; i++)
    vec.emplace_back(i);

  auto erased_it = vec.erase(vec.begin() + 2);
  CHECK(vec[2].val == 3);
  CHECK(test_content::live_count == 9);
  CHECK(vec.size() == 9);
  CHECK(erased_it->val = 3);

  erased_it = vec.erase(vec.end() - 1);
  CHECK(vec[7].val == 8);
  CHECK(test_content::live_count == 8);
  CHECK(vec.size() == 8);
  CHECK(erased_it == vec.end());

  erased_it = vec.erase(vec.begin());
  CHECK(vec[0].val == 1);
  CHECK(test_content::live_count == 7);
  CHECK(vec.size() == 7);
  CHECK(erased_it->val = 2);
}

void test_vec_erase_range_begin()
{
  vec_t<test_content> vec;

  for (int32_t i = 0; i < 10; i++)
    vec.emplace_back(i);

  auto erased_it = vec.erase(vec.begin(), vec.begin() + 5);
  CHECK(vec.size() == 5);
  CHECK(vec[0].val == 5);
  CHECK(erased_it->val == 5);
}

void test_vec_erase_range_end()
{
  vec_t<test_content> vec;

  for (int32_t i = 0; i < 10; i++)
    vec.emplace_back(i);

  auto erased_it = vec.erase(vec.end() - 5, vec.end());
  CHECK(vec.size() == 5);
  CHECK(vec[4].val == 4);
  CHECK(erased_it == vec.end());
}

void test_vec_erase_range_middle()
{
  vec_t<test_content> vec;

  for (int32_t i = 0; i < 10; i++)
    vec.emplace_back(i);

  auto erased_it = vec.erase(vec.begin() + 2, vec.begin() + 5);
  CHECK(vec.size() == 7);
  CHECK(erased_it->val == 5);

  CHECK(vec[0].val == 0);
  CHECK(vec[1].val == 1);
  CHECK(vec[2].val == 5);
  CHECK(vec[3].val == 6);
  CHECK(vec[4].val == 7);
  CHECK(vec[5].val == 8);
  CHECK(vec[6].val == 9);
}

#undef vec_t

extern "C" void run_c_tests();

int main()
{
  run_c_tests();

  test_vec_basic();
  test_vec_empty();
  test_vec_push_pop();
  test_vec_clear();
  test_vec_realloc();
  test_vec_shrink_to_fit();
  test_vec_insert_begin();
  test_vec_insert_middle();
  test_vec_move_insert_range();
  test_vec_erase_simple();
  test_vec_erase_range_begin();
  test_vec_erase_range_end();
  test_vec_erase_range_middle();

  fputs("All tests passed!\n", stderr);
  return 0;
}
