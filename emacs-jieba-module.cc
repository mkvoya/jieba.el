#include <string>
#include <vector>
#include <thread>
using namespace std;

#include "emacs-module.h"

#include "cppjieba/Jieba.hpp"

#ifdef DEBUG
#define dprintf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define dprintf(fmt, ...)
#endif

int plugin_is_GPL_compatible = 1;

/* Frequently-used symbols. */
static emacs_value Qnil;
static emacs_value Qt;

static const string DICT_PATH = "cppjieba/dict/jieba.dict.utf8";
static const string HMM_PATH = "cppjieba/dict/hmm_model.utf8";
static const string USER_DICT_PATH = "cppjieba/dict/user.dict.utf8";
static const string IDF_PATH = "cppjieba/dict/idf.utf8";
static const string STOP_WORD_PATH = "cppjieba/dict/stop_words.utf8";

static cppjieba::Jieba *jieba = nullptr;
static thread initer;

void init_jieba(const string &base_dir)
{
  jieba = new cppjieba::Jieba(base_dir + DICT_PATH,
                              base_dir + HMM_PATH,
                              base_dir + USER_DICT_PATH,
                              base_dir + IDF_PATH,
                              base_dir + STOP_WORD_PATH);
}

static string
emacs_get_string(emacs_env *env, emacs_value str)
{
  ptrdiff_t len;

  bool succ = env->copy_string_contents(env, str, NULL, &len);
  if (!succ)
    throw "error getting the string length.\n";

  string s;
  // Reserve the space, len already includes the trailing \0.
  s.resize(len);

  succ = env->copy_string_contents(env, str, (char *)s.data(), &len);
  if (!succ)
    throw "error getting the string content.\n";

  // Remove the trailing \0.
  s.resize(len - 1);
  return s;
}

static emacs_value
jieba_do_split(emacs_env *env, ptrdiff_t n, emacs_value *args, void *_) noexcept
{
  if (!jieba)
    initer.join();

  vector<string> words;
  try {
    string s = emacs_get_string(env, args[0]);
    // cout << s << endl;
    jieba->Cut(s, words, true);
    // cout << limonp::Join(words.begin(), words.end(), "/") << endl;
  } catch (const char *msg) {
    fputs(msg, stderr);
    return Qnil;
  }

  size_t len = words.size();
  // cout << len << endl;
  // (make-vector len nil)
  emacs_value __args[] = { env->make_integer(env, len), Qnil };
  emacs_value vec = env->funcall(env, env->intern(env, "make-vector"), 2, __args);
  for (int i = 0; i < len; ++i) {
    emacs_value w = env->make_string(env, words[i].c_str(), words[i].size());
    env->vec_set(env, vec, i, w);
  }
  return vec;
}


int
emacs_module_init (struct emacs_runtime *ert) noexcept
{
  emacs_env *env = ert->get_environment (ert);

  // Symbols
  Qt = env->make_global_ref (env, env->intern(env, "t"));
  Qnil = env->make_global_ref (env, env->intern(env, "nil"));

  try {
    emacs_value d = env->funcall(env, env->intern(env, "jieba--current-dir"),
                                 0, nullptr);
    string base_dir = emacs_get_string(env, d);
    // cout << base_dir << endl;

    // It takes too long to construct a jieba object, thus we do that in another thread.
    initer = std::thread(init_jieba, std::move(base_dir));

  } catch (const char *msg) {
    fputs(msg, stderr);
    return -1;
  }

  emacs_value Femacs_do_split;
  Femacs_do_split = env->make_function (env, 1, 1, /* Take exactly 1 parameter -- the string to split. */
                                        jieba_do_split, "Split a given string via jieba.", NULL);
  emacs_value symbol = env->intern (env, "emacs-jieba--do-split"); /* the symbol */
  emacs_value args[] = {symbol, Femacs_do_split};
  env->funcall (env, env->intern (env, "defalias"), 2, args);

  emacs_value Qfeat = env->intern (env, "emacs-jieba-module");
  emacs_value Qprovide = env->intern (env, "provide");
  env->funcall (env, Qprovide, 1, (emacs_value[]){Qfeat});

  return 0;
}
