
all : emacs-jieba-module.so
CC=clang++

CFLAGS += -Icppjieba/include -Icppjieba/deps -std=c++11

debug: CFLAGS += -DDEBUG -g
debug: emacs-jieba-module.so

emacs-jieba-module.so : emacs-jieba-module.cc
	$(CC) -shared $(CFLAGS) $(LDFLAGS) -o $@ $^

clean :
	$(RM) emacs-jieba-module.so

.PHONY : clean all
