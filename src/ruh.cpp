/*
 *  aRe yoU Human
 *
 *  (c) Holger Berger 2015
 *
 *  workspace++ is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  workspace++ is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with workspace++.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdlib.h>
#include <termcap.h>
#include <vector>
#include <string>
#include <iostream>

using namespace std;

// vector of japanese syllables
// vector<string> syllable = {
const char * syllable[] = {
 	"a","i","u","e","o",
	"ka","ki","ku","ke","ko",
	"sa","shi","su","se","so",
	"ta","chi","tsu","te","to",
	"na","ni","nu","ne","no",
	"ha","hi","fu","he","ho",
	"ma","mi","mu","me","mo",
	"ya","yu","yo",
	"ra","ri","ru","re","ro",
	"wa","wi","we","wo","n",
	NULL
};

const int syllablesize = 48;

// print a character r times
void repstr(char *c, int r) {
	for(int i=0; i<r; i++) {
		cout << c;
	}
}

// check if person behind tty is human
bool ruh() {
	srand (time(NULL));
	int syllables = 3 + rand() % 5;

	vector<string> word;
	string word_as_string;

	for(int i=0; i<syllables; i++) {
		int r = rand() % syllablesize;
		word.push_back(syllable[r]);
		word_as_string.append(syllable[r]);
	}

	int r = tgetent(NULL,getenv("TERM"));
	if (r != 1 ) {
		cerr << "unsupported terminal!" << endl;
		return false;
	}
	char *left  = tgetstr((char*)"le",NULL);
	char *right = tgetstr((char*)"nd",NULL);


	cout << "to verify that you are human, please type '";

	if (rand() & 1) {	
		// scheme 1
		cout << word[0];
		repstr(right, word[1].size());
		cout << word[2];
		repstr(left, word[1].size()+word[2].size());
		cout << word[1];
		repstr(right, word[2].size());
		for(int i=3; i<word.size(); i++) {
			cout << word[i];
		}
	} else {
		// scheme 2
		repstr(right, word[0].size());
		cout << word[1];
		repstr(left, word[0].size()+word[1].size());
		cout << word[0];
		repstr(right, word[1].size());
		for(int i=2; i<word.size(); i++) {
			cout << word[i];
		}
	}
	cout << "': " << flush;
	
	string line;
	getline(cin, line);
	
	if(line==word_as_string) {
		cout << "you are human\n";
		return true;
	} else {
		cout << "not sure if you are human\n";
		return false;
	}
	
}

