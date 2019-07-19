#include <utility>
#include <math.h>
#include <cassert>
#include <functional>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include "hmm.hpp"
#include "emissionprobabilitycomputer.hpp"

#include <iostream>

using namespace std;

void print_column(vector<long double>* column, ColumnIndexer* indexer) {
	for (size_t i = 0; i < column->size(); ++i) {
		pair<size_t,size_t> paths = indexer->get_paths(i);
		cout << setprecision(15) << column->at(i) << " paths: " << paths.first << " " <<  paths.second << endl;
	}
	cout << "" << endl;
}


HMM::HMM(vector<UniqueKmers*>* unique_kmers, const vector<Variant>& variants, double recombrate)
	:unique_kmers(unique_kmers),
	 genotyping_result(unique_kmers->size())
{
	size_t size = this->unique_kmers->size();

	// construct TransitionProbabilityComputers
	init(this->transition_prob_computers, size-1);	
	for (size_t i = 1; i < size; ++i) {
		size_t prev_pos = this->unique_kmers->at(i-1)->get_variant_position();
		size_t cur_pos = this->unique_kmers->at(i)->get_variant_position();
		TransitionProbabilityComputer* t = new TransitionProbabilityComputer(prev_pos, cur_pos, recombrate);
		this->transition_prob_computers.at(i-1) = t;
	}
	this->previous_backward_column = nullptr;

	cerr << "Indexing the columns ..." << endl;
	index_columns();
	cerr << "Computing forward probabilities ..." << endl;
	compute_forward_prob();
//	for(size_t i = 0; i < this->forward_columns.size(); ++i) {
//		cout << "forward " << variants.at(i).get_start_position() << endl;
//		if (this->forward_columns.at(i) == nullptr) {
//			cout << "NULL" << endl;
//			continue;
//		}
//		print_column(this->forward_columns.at(i), this->column_indexers.at(i));
//	}
	cerr << "Computing backward probabilities ..." << endl;
	compute_backward_prob(variants);

//	cout << "genotype likelihoods" << endl;
//	for (size_t i = 0; i < this->genotyping_result.size(); ++i) {
//		cout << genotyping_result[i] << endl;
//	}

	cerr << "Computing Viterbi path ..." << endl;
	compute_viterbi_path(variants);
//	for(size_t i = 0; i < this->viterbi_columns.size(); ++i){
//		print_column(this->viterbi_columns.at(i), this->column_indexers.at(i));
//	}
}

HMM::~HMM(){
	init(this->forward_columns,0);
	if (this->previous_backward_column != nullptr) delete this->previous_backward_column;
	init(this->viterbi_columns,0);
	init(this->transition_prob_computers,0);
	init(this->viterbi_backtrace_columns,0);
	init(this->column_indexers, 0);
}

void HMM::index_columns() {
	size_t column_count = this->unique_kmers->size();
	init(column_indexers, column_count);
	// do one forward pass to compute ColumnIndexers
	for (size_t column_index = 0; column_index < column_count; ++ column_index) {
		// get path ids of current column
		vector<size_t> current_path_ids;
		vector<unsigned char> current_allele_ids;
		this->unique_kmers->at(column_index)->get_path_ids(current_path_ids, current_allele_ids);
		size_t nr_paths = current_path_ids.size();

		if (nr_paths == 0) {
			ostringstream oss;
			oss << "HMM::index_columns: column " << column_index << " is not covered by any paths.";
			throw runtime_error(oss.str());
		}

		// the ColumnIndexer to be filled
		ColumnIndexer* column_indexer = new ColumnIndexer(column_index, nr_paths*nr_paths);

		// iterate over all cells
		for (size_t p1 = 0; p1 < nr_paths; ++p1) {
			for (size_t p2 = 0; p2 < nr_paths; ++p2) {
				size_t path_id1 = current_path_ids[p1];
				size_t path_id2 = current_path_ids[p2];
				unsigned char allele_id1 = current_allele_ids[p1];
				unsigned char allele_id2 = current_allele_ids[p2];
				column_indexer->insert(make_pair(path_id1,path_id2), make_pair(allele_id1,allele_id2));
			}
		}
		// store the ColummIndexer
		this->column_indexers.at(column_index) = column_indexer;
	}
}

void HMM::compute_forward_prob() {
	size_t column_count = this->unique_kmers->size();
	init(this->forward_columns, column_count);
	
	// forward pass
	size_t k = (size_t) sqrt(column_count);
	for (size_t column_index = 0; column_index < column_count; ++column_index) {;
		compute_forward_column(column_index);
		// sparse table: check whether to delete previous column
		if ( (k > 1) && (column_index > 0) && (((column_index - 1)%k != 0)) ) {
			delete this->forward_columns[column_index-1];
			this->forward_columns[column_index-1] = nullptr;
		}
	}
}

void HMM::compute_backward_prob(const vector<Variant>& variants) {
	size_t column_count = this->unique_kmers->size();
	if (this->previous_backward_column != nullptr) {
		delete this->previous_backward_column;
		this->previous_backward_column = nullptr;
	}

	// backward pass
	for (int column_index = column_count-1; column_index >= 0; --column_index) {
		compute_backward_column(column_index, variants);
	}
}

void HMM::compute_viterbi_path(const vector<Variant>& variants) {
	size_t column_count = this->unique_kmers->size();
	init(this->viterbi_columns, column_count);
	init(this->viterbi_backtrace_columns, column_count);

	// perform viterbi algorithm
	size_t k = (size_t) sqrt(column_count);
	for (size_t column_index = 0; column_index < column_count; ++column_index) {
		compute_viterbi_column(column_index);
		// sparse table: check whether to delete previous column
		if ((k > 1) && (column_index > 0) && (((column_index - 1)%k != 0)) ) {
			delete this->viterbi_columns[column_index-1];
			this->viterbi_columns[column_index-1] = nullptr;
			delete this->viterbi_backtrace_columns[column_index-1];
			this->viterbi_backtrace_columns[column_index-1] = nullptr;
		}
	}

	// find best value (+ index) in last column
	size_t best_index = 0;
	long double best_value = 0.0L;
	vector<long double>* last_column = this->viterbi_columns.at(column_count-1);
	assert (last_column != nullptr);
	for (size_t i = 0; i < last_column->size(); ++i) {
		long double entry = last_column->at(i);
		if (entry >= best_value) {
			best_value = entry;
			best_index = i;
		}
	}

	// backtracking
	size_t column_index = column_count - 1;
	while (true) {
		pair<size_t, size_t> path_ids = this->column_indexers.at(column_index)->get_paths(best_index);

		// columns might have to be re-computed
		if (this->viterbi_backtrace_columns[column_index] == nullptr) {
			size_t j = column_index / k*k;
			assert (this->viterbi_columns[j] != nullptr);
			for (j = j+1; j<=column_index; ++j) {
				compute_viterbi_column(j);
			}
		}

		// get alleles these paths correspond to
		unsigned char allele1 = variants.at(column_index).get_allele_on_path(path_ids.first);
		unsigned char allele2 = variants.at(column_index).get_allele_on_path(path_ids.second);
		this->genotyping_result.at(column_index).add_first_haplotype_allele(allele1);
		this->genotyping_result.at(column_index).add_second_haplotype_allele(allele2);

		if (column_index == 0) break;

		// update best index 
		best_index = this->viterbi_backtrace_columns.at(column_index)->at(best_index);
		column_index -= 1;
	}
}

void HMM::compute_forward_column(size_t column_index) {
	assert(column_index < this->unique_kmers->size());

	// check whether column was computed already
	if (this->forward_columns[column_index] != nullptr) return;

	// get previous column and previous path ids (if existent)
	vector<long double>* previous_column = nullptr;
	ColumnIndexer* previous_indexer = nullptr;
	TransitionProbabilityComputer* transition_probability_computer = nullptr;
	if (column_index > 0) {
		previous_column = this->forward_columns[column_index-1];
		previous_indexer = this->column_indexers.at(column_index-1);
		transition_probability_computer = this->transition_prob_computers.at(column_index-1);
	}

	// construct new column
	vector<long double>* current_column = new vector<long double>();

	// get ColumnIndexer
	ColumnIndexer* column_indexer = column_indexers.at(column_index);
	assert (column_indexer != nullptr);

	// emission probability computer
	EmissionProbabilityComputer emission_probability_computer(this->unique_kmers->at(column_index), column_indexer);

	// normalization
	long double normalization_sum = 0.0L;

	// iterate over all pairs of current paths
	for (size_t i = 0; i < column_indexer->get_size(); ++i) {
		pair<size_t,size_t> path_ids = column_indexer->get_paths(i);
		long double previous_cell = 0.0L;
		if (column_index > 0) {
			// iterate over all cells in previous column
			for (size_t j = 0; j < previous_indexer->get_size(); ++j) {

				// forward probability of previous cell
				long double prev_forward = previous_column->at(j);

				// determine path ids previous cell corresponds to
				pair<size_t,size_t> prev_path_ids = previous_indexer->get_paths(j); 

				// determine transition probability
				long double transition_prob = transition_probability_computer->compute_transition_prob(prev_path_ids.first, prev_path_ids.second, path_ids.first, path_ids.second);
				previous_cell += prev_forward * transition_prob;
			}
		} else {
			previous_cell = 1.0L;
		}
		// determine emission probability
		long double emission_prob = emission_probability_computer.get_emission_probability(i);

		// set entry of current column
		long double current_cell = previous_cell * emission_prob;
		current_column->push_back(current_cell);
		normalization_sum += current_cell;
	}

	// normalize the entries in current column to sum up to 1
	transform(current_column->begin(), current_column->end(), current_column->begin(), bind(divides<long double>(), placeholders::_1, normalization_sum));

	// store the column
	this->forward_columns.at(column_index) = current_column;
}

void HMM::compute_backward_column(size_t column_index, const vector<Variant>& variants) {
	size_t column_count = this->unique_kmers->size();
	assert(column_index < column_count);

	// get previous indexers and probabilitycomputers
	ColumnIndexer* previous_indexer = nullptr;
	TransitionProbabilityComputer* transition_probability_computer = nullptr;
	EmissionProbabilityComputer* emission_probability_computer = nullptr;
	vector<long double>* forward_column = this->forward_columns.at(column_index);

	if (column_index < column_count-1) {
		assert (this->previous_backward_column != nullptr);
		transition_probability_computer = this->transition_prob_computers.at(column_index);
		previous_indexer = this->column_indexers.at(column_index+1);
		emission_probability_computer = new EmissionProbabilityComputer(this->unique_kmers->at(column_index+1), previous_indexer);

		// get forward probabilities (needed for computing posteriors
		if (forward_column == nullptr) {
			// compute index of last column stored
			size_t k = (size_t)sqrt(column_count);
			size_t next = min((size_t) ( (column_index / k) * k ), column_count-1);
			for (size_t j = next+1; j <= column_index; ++j) {
				compute_forward_column(j);
			}
		}

		forward_column = this->forward_columns.at(column_index);
		assert (forward_column != nullptr);
	}

	// construct new column
	vector<long double>* current_column = new vector<long double>();

	// get ColumnIndexer
	ColumnIndexer* column_indexer = column_indexers.at(column_index);
	assert (column_indexer != nullptr);

	// normalization
	long double normalization_sum = 0.0L;

	// normalization of forward-backward
	long double normalization_f_b = 0.0L;

	// iterate over all pairs of current paths
	for (size_t i = 0; i < column_indexer->get_size(); ++i) {
		pair<size_t, size_t> path_ids = column_indexer->get_paths(i);
		long double current_cell = 0.0L;
		if (column_index < column_count - 1) {
			// iterate over previous column (ahead of this)
			for (size_t j = 0; j < previous_indexer->get_size(); ++j) {
				long double prev_backward = this->previous_backward_column->at(j);
				// determine path ids previous cell corresponds to
				pair<size_t,size_t> prev_path_ids = previous_indexer->get_paths(j);
				// determine transition probability
				long double transition_prob = transition_probability_computer->compute_transition_prob(path_ids.first, path_ids.second, prev_path_ids.first, prev_path_ids.second);
				current_cell += prev_backward * transition_prob * emission_probability_computer->get_emission_probability(j);
			}
		} else {
			current_cell = 1.0L;
		}
	
		// store computed backward prob in column
		current_column->push_back(current_cell);
		normalization_sum += current_cell;

		// compute forward_prob * backward_prob
		long double forward_backward_prob = forward_column->at(i) * current_cell;
		normalization_f_b += forward_backward_prob;

		// get alleles corresponding to paths
		unsigned char allele1 = variants.at(column_index).get_allele_on_path(path_ids.first);
		unsigned char allele2 = variants.at(column_index).get_allele_on_path(path_ids.second);
		this->genotyping_result.at(column_index).add_to_likelihood(allele1, allele2, forward_backward_prob);
	}

	transform(current_column->begin(), current_column->end(), current_column->begin(), bind(divides<long double>(), placeholders::_1, normalization_sum));

//	cout << "FORWARD COLUMN: " << variants.at(column_index).get_start_position() << endl;
//	print_column(forward_column, column_indexer);

//	cout << "BACKWARD COLUMN: " << variants.at(column_index).get_start_position() << endl;
//	print_column(current_column, column_indexer);

	// store computed column (needed for next step)
	if (this->previous_backward_column != nullptr) {
		delete this->previous_backward_column;
		this->previous_backward_column = nullptr;
	}
	this->previous_backward_column = current_column;
	if (emission_probability_computer != nullptr) delete emission_probability_computer;

	// normalize the GenotypingResults likelihoods 
	this->genotyping_result.at(column_index).divide_likelihoods_by(normalization_f_b);
}

void HMM::compute_viterbi_column(size_t column_index) {
	assert(column_index < this->unique_kmers->size());

	// check whether column was computed already
	if (this->viterbi_columns[column_index] != nullptr) return;

	// get previous column and previous path ids (if existent)
	vector<long double>* previous_column = nullptr;
	ColumnIndexer* previous_indexer = nullptr;
	TransitionProbabilityComputer* transition_probability_computer = nullptr;
	if (column_index > 0) {
		previous_column = this->viterbi_columns[column_index-1];
		previous_indexer = this->column_indexers.at(column_index-1);
		transition_probability_computer = this->transition_prob_computers.at(column_index-1);
	}

	// construct new column
	vector<long double>* current_column = new vector<long double>();

	// get ColumnIndexer
	ColumnIndexer* column_indexer = this->column_indexers.at(column_index);
	assert (column_indexer != nullptr);

	// emission probability computer
	EmissionProbabilityComputer emission_probability_computer(this->unique_kmers->at(column_index), column_indexer);

	// normalization 
	long double normalization_sum = 0.0L;

	// backtrace table
	vector<size_t>* backtrace_column = new vector<size_t>();

	// iterate over all pairs of current paths
	for (size_t i = 0; i < column_indexer->get_size(); ++i) {
		long double previous_cell = 0.0L;
		pair<size_t,size_t> path_ids = column_indexer->get_paths(i);
		if (column_index > 0) {
			//iterate over previous cells and determine maximum
			long double max_value = 0.0L;
			size_t max_index = 0;
			for (size_t j = 0; j < previous_indexer->get_size(); ++j) {

				// probability of previous cell
				long double prev_prob = previous_column->at(j);

				// determine path ids previous cell corresponds to
				pair<size_t,size_t> prev_path_ids = previous_indexer->get_paths(j);

				// determine transition probability
				long double transition_prob = transition_probability_computer->compute_transition_prob(prev_path_ids.first, prev_path_ids.second, path_ids.first, path_ids.second);
				prev_prob *= transition_prob;
				if (prev_prob >= max_value) {
					max_value = prev_prob;
					max_index = j;
				}
			}
			previous_cell = max_value;
			backtrace_column->push_back(max_index);
		} else {
			previous_cell = 1.0L;
		}

		// determine emission probability
		long double emission_prob = emission_probability_computer.get_emission_probability(i);

		// set entry of current column
		long double current_cell = previous_cell * emission_prob;
		current_column->push_back(current_cell);
		normalization_sum += current_cell;
	}

	// normalize the entries in current column
	transform(current_column->begin(), current_column->end(), current_column->begin(), bind(divides<long double>(), placeholders::_1, normalization_sum));

	// store the column
	this->viterbi_columns.at(column_index) = current_column;
	if (column_index > 0) assert(backtrace_column->size() == column_indexer->get_size());
	this->viterbi_backtrace_columns.at(column_index) = backtrace_column;
}

const vector<GenotypingResult>& HMM::get_genotyping_result() const {
	return this->genotyping_result;
}
