#include "login_form.h"
#include <stdexcept>
#include <iostream>
using F=mgo2win::LoginForm;
void require(bool b){if(!b)throw std::runtime_error("Login form contract");}
int main(){try{
 F f;f.focus(2);f.key(F::confirm);require(f.focus()==0&&!f.notice().empty());
 for(wchar_t c:std::wstring(L"test_id"))f.character(c);
 f.key(F::home);f.key(F::eraseForward);require(f.display(0)==L"est_id");f.key(F::end);f.key(F::eraseBack);require(f.display(0)==L"est_i");
 f.key(F::selectAll);f.character(L'A');require(f.display(0)==L"A");
 for(int i=0;i<100;++i)f.character(L'b');require(f.length(0)==64);
 f.key(F::confirm);require(f.focus()==1);f.character(L'x');f.character(L'y');require(f.display(1)==L"••");
 f.key(F::left);f.character(L'z');require(f.length(1)==3&&f.caret()==2);
 f.key(F::selectAll);f.key(F::eraseBack);require(f.length(1)==0);
 f.focus(2);f.key(F::confirm);require(f.focus()==1);
 f.character(L'p');f.key(F::confirm);f.key(F::confirm);require(f.attempts()==3&&f.notice().find(L"送信されていません")!=std::wstring::npos);
 f.key(F::cancel);require(f.back()&&f.length(0)==0&&f.length(1)==0);
 f.clear();require(!f.back());f.key(F::previous);require(f.focus()==6);f.key(F::confirm);require(f.save_mode()==2);f.focus(3);f.key(F::confirm);require(f.back());
 std::cout<<"Login empty checks, masked password, cursor edits, length bound, selection replacement and back clearing passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
