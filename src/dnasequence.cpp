#include <stdexcept>
#include "dnasequence.hpp"
#include <iostream>

using namespace std;

unsigned char encode (char base) {
	switch (base) {
		case 'A': return 0;
		case 'a': return 0;
		case 'C': return 1;
		case 'c': return 1;
		case 'G': return 2;
		case 'g': return 2;
		case 'T': return 3;
		case 't': return 3;
		default: return 4;
	}
}

unsigned char complement (unsigned char base) {
	switch(base) {
		case 0: return 3;
		case 1: return 2;
		case 2: return 1;
		case 3: return 0;
		case 4: return 4;
		default: throw runtime_error("DnaSequence::complement: invalid base.");
	}
}

char decode (unsigned char number) {
	switch (number) {
		case 0: return 'A';
		case 1: return 'C';
		case 2: return 'G';
		case 3: return 'T';
		default: return 'N';
	}
}

DnaSequence::DnaSequence() 
	:length(0)
{}

DnaSequence::DnaSequence(string& sequence)
	:length(0)
{
	this->append(sequence);	
}

void DnaSequence::append(string& seq) {
	for (auto base : seq) {
		unsigned char number = encode(base);
		if (this->length % 2 == 0) {
			this->sequence.push_back(number << 4);
		} else {
			size_t index = this->length / 2;
			unsigned char updated = (this->sequence[index]) | number;
			this->sequence[index] = updated;
		}
		this->length += 1;
	}
}

void DnaSequence::append(DnaSequence seq) {
	if (this->length % 2 == 0) {
		for (size_t i = 0; i < seq.sequence.size(); ++i) {
			this->sequence.push_back(seq.sequence.at(i));
		}
	} else {
		if (seq.size() == 0) return;
		unsigned char current = this->sequence.at(this->sequence.size()-1);
		this->sequence.pop_back();
		for (size_t i = 0; i < seq.size(); ++i) {
			unsigned char s = seq.sequence.at(i/2);
			if (i % 2 == 0) {
				current |= (s >> 4);
				this->sequence.push_back(current);
			} else {
				current = (s << 4);
			}
		}
		if (seq.size() % 2 == 0) this->sequence.push_back(current);
	}
	this->length += seq.length;
}

void DnaSequence::reverse() {
	vector<unsigned char> reversed;
	if (this->length % 2 == 0) {
		for (vector<unsigned char>::reverse_iterator it = this->sequence.rbegin(); it != this->sequence.rend(); ++it) {
			unsigned char reversed_element = ((*it) >> 4) | ((*it) << 4);
			reversed.push_back(reversed_element);
		}
	} else {
		unsigned char current = this->sequence[this->sequence.size() - 1];
		this->sequence.pop_back();
		for (vector<unsigned char>::reverse_iterator it = this->sequence.rbegin(); it != this->sequence.rend(); ++it) {
			unsigned char second = (*it) << 4;
			reversed.push_back( (second >> 4) | current );
			current = (*it) & 240;
		}
		reversed.push_back(current);
	}
	this->sequence = move(reversed);
}

void DnaSequence::reverse_complement() {
	vector<unsigned char> reverse_compl;
	if (this->length % 2 == 0) {
		for (vector<unsigned char>::reverse_iterator it = this->sequence.rbegin(); it != this->sequence.rend(); ++it) {
			unsigned char first = complement((*it) >> 4);
			unsigned char second = complement((*it) & 15);
			reverse_compl.push_back( (second << 4) | first);
		}
	} else {
		unsigned char current = complement(this->sequence[this->sequence.size() - 1] >> 4) << 4;
		this->sequence.pop_back();
		for (vector<unsigned char>::reverse_iterator it = this->sequence.rbegin(); it != this->sequence.rend(); ++it) {
			unsigned char second = complement((*it) & 15);
			unsigned char first = complement((*it) >> 4);
			reverse_compl.push_back(current | second);
			current = (first << 4);
		}
		reverse_compl.push_back(current);
	}
	this->sequence = move(reverse_compl);
}

char DnaSequence::operator[](size_t position) const {
	if (position >= this->length) {
		throw runtime_error("DnaSequence::operator[]: index out of bounds.");
	}

	unsigned char number = this->sequence[position / 2];
	if (position % 2 == 0) {
		return decode(number >> 4);
	} else {
		return decode(number & 15);
	}
}

DnaSequence DnaSequence::base_at(size_t position) const {
	if (position >= this->length) {
		throw runtime_error("DnaSequence::base_at: index out of bounds.");
	}

	DnaSequence result;
	unsigned char number = this->sequence[position / 2];
	if (position % 2 == 0) {
		result.sequence.push_back(number & 240);
	} else {
		result.sequence.push_back(number << 4);
	}
	result.length = 1;
	return result;
}

size_t DnaSequence::size() const {
	return this->length;
}

void DnaSequence::substr(size_t start, size_t end, string& result) const {
	result.clear();
	for (size_t i = start; i < end; ++i) {
		result += (*this)[i];
	}
}

void DnaSequence::substr(size_t start, size_t end, DnaSequence& result) const {
	vector<unsigned char> substring;
	if (start % 2 == 0) {
		size_t start_index = start / 2;
		size_t steps = ((end + 1) -start)/ 2;
		substring = vector<unsigned char>(this->sequence.begin() + start_index, this->sequence.begin() + start_index + steps);
		if (end % 2 != 0) substring[substring.size()-1] &= 240;
	} else {
		unsigned char current = (this->sequence[start/2]) << 4;
		for (size_t i = start+1; i < end; i+=2) {
			unsigned char element = this->sequence[i/2];
			current |= (element >> 4);
			substring.push_back(current);
			current = element <<  4;
		}
		if (end % 2 == 0) {
			substring.push_back(current);	
		}
	}
	result.sequence = move(substring); 
	result.length = (end - start);
}

string DnaSequence::to_string() const {
	string result;
	for (size_t i = 0; i < this->length; ++i) {
		result += (*this)[i];
	}
	return result;
}

void DnaSequence::clear() {
	this->sequence.clear();
	this->length = 0;
}

bool DnaSequence::operator<(const DnaSequence& dna) const {
	return (this->sequence < dna.sequence);
}

bool operator==(const DnaSequence& dna1, const DnaSequence& dna2) {
	return (dna1.sequence == dna2.sequence);
}

bool operator!=(const DnaSequence& dna1, const DnaSequence& dna2) {
	return !(dna1 == dna2);
}
