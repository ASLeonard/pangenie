#include "catch.hpp"
#include "../src/variantreader.hpp"
#include <vector>
#include <string>

using namespace std;

TEST_CASE("VariantReader get_allele_string", "[VariantReader get_allele_string]") {
	string vcf = "../tests/data/small1.vcf";
	string fasta = "../tests/data/small1.fa";
	VariantReader v(vcf, fasta, 10);
	REQUIRE(v.nr_of_genomic_kmers() == 2440);
	REQUIRE(v.get_kmer_size() == 10);
	REQUIRE(v.size_of("chrA") == 7);
	REQUIRE(v.size_of("chrB") == 2);
	REQUIRE(v.get_variant("chrA", 2).nr_of_alleles() == 3);

	REQUIRE(v.get_variant("chrA", 0).get_allele_string(0) == "GGAATTCCGACATAAGTTA");
	REQUIRE(v.get_variant("chrA", 0).get_allele_string(1) == "GGAATTCCGTCATAAGTTA");

	REQUIRE(v.get_variant("chrA", 1).get_allele_string(0) == "CCTTAGCTACGAAGCCAGT");
	REQUIRE(v.get_variant("chrA", 1).get_allele_string(1) == "CCTTAGCTAGGGGGAAGCCAGT");

	REQUIRE(v.get_variant("chrA", 2).get_allele_string(0) == "GAAGCCAGTGCCCCGAGACGGCCAAA");
	REQUIRE(v.get_variant("chrA", 2).get_allele_string(1) == "GAAGCCAGTTCCCCGAGACGGCCAAA");
	REQUIRE(v.get_variant("chrA", 2).get_allele_string(2) == "GAAGCCAGTTCCCCTACGGCCAAA");
	REQUIRE(v.get_variant("chrA", 2).nr_of_paths() == 4);

	REQUIRE(v.get_variant("chrA", 3).get_allele_string(0) == "ACGTCCGTTCAGCCTTAGC");
	REQUIRE(v.get_variant("chrA", 3).get_allele_string(1) == "ACGTCCGTTTAGCCTTAGC");

	REQUIRE(v.get_variant("chrA", 4).get_allele_string(0) == "CCGATTTTCTTGTGCTATA");
	REQUIRE(v.get_variant("chrA", 4).get_allele_string(1) == "CCGATTTTCCTGTGCTATA");

	REQUIRE(v.get_variant("chrA", 5).get_allele_string(0) == "GGAGGGTATGAAGCCATCAC");
	REQUIRE(v.get_variant("chrA", 5).get_allele_string(1) == "GGAGGGTATTCAGCCATCAC");

        REQUIRE(v.get_variant("chrA", 6).get_allele_string(0) == "TGTGGACTTATTTGGCTAA");
        REQUIRE(v.get_variant("chrA", 6).get_allele_string(1) == "TGTGGACTTGTTTGGCTAA");

	REQUIRE(v.get_variant("chrB", 0).get_allele_string(0) == "CCACTTCATCAAGACACAA");
	REQUIRE(v.get_variant("chrB", 1).get_allele_string(0) == "GAGTATTTTGATCATAAAT");

	v.write_path_segments("/MMCI/TM/scratch/jebler/pgg-typer/pggtyper/pggtyper/tests/data/small1-segments.fa");
}

TEST_CASE("VariantReader write_path_segments", "[VariantReader write_path_segments]") {
	string vcf = "../tests/data/small1.vcf";
	string fasta = "../tests/data/small1.fa";

	// read variants from VCF file
	VariantReader v(vcf, fasta, 10);
	v.write_path_segments("../tests/data/small1-segments.fa");

	// compare reference segments to expected sequences
	vector<string> computed = {};
	vector<string> expected = {};

	// read expected reference segments from file
	ifstream expected_sequences("../tests/data/small1-expected-ref-segments.fa");
	string line;
	while (getline(expected_sequences, line)) {
		size_t start = line.find_first_not_of(" \t\r\n");
		size_t end = line.find_last_not_of(" \t\r\n");
		line = line.substr(start, end-start+1);
		expected.push_back(line);
	}

	// read computed reference segments from file
	bool read_next = false;
	ifstream computed_sequences("../tests/data/small1-segments.fa");
	while (getline(computed_sequences, line)) {
		if (line.size() == 0) continue;
		if (line[0] == '>') {
			if (line.find("reference") != string::npos) {
				read_next = true;
			} else {
				read_next = false;
			}
			continue;
		}
		size_t start = line.find_first_not_of(" \t\r\n");
		size_t end = line.find_last_not_of(" \t\r\n");
		line = line.substr(start, end-start+1);
		if (read_next) computed.push_back(line);
	}

	REQUIRE(expected.size() == computed.size());
	for (size_t i = 0; i < expected.size(); ++i) {
		REQUIRE(expected[i] == computed[i]);
	}

}

TEST_CASE("VariantReader write_genotypes_of", "[VariantReader write_genotypes_of]") {
	string vcf = "../tests/data/small1.vcf";
	string fasta = "../tests/data/small1.fa";

	// read variants from VCF file
	VariantReader v(vcf, fasta, 10, "HG0");

	vector<string> chromosomes;
	vector<string> expected_chromosomes = {"chrA", "chrB"};
	v.get_chromosomes(&chromosomes);

	REQUIRE(chromosomes.size() == expected_chromosomes.size());
	REQUIRE(chromosomes[0] == expected_chromosomes[0]);
	REQUIRE(chromosomes[1] == expected_chromosomes[1]);

	// generate a GenotypingResult for chrA
	vector<GenotypingResult> genotypes_chrA(7);
	for (size_t i = 0; i < 7; ++i) {
		if (i == 2) continue;
		GenotypingResult r;
		r.add_to_likelihood(0,0,0.2);
		r.add_to_likelihood(0,1,0.7);
		r.add_to_likelihood(1,1,0.1);
		genotypes_chrA[i] = r;
	}

	// third variant is multiallelic
	GenotypingResult r;
	r.add_to_likelihood(0,0,0.2);
	r.add_to_likelihood(0,1,0.0);
	r.add_to_likelihood(0,2,0.2);
	r.add_to_likelihood(1,1,0.0);
	r.add_to_likelihood(1,2,0.5);
	r.add_to_likelihood(2,2,0.1);
	r.add_first_haplotype_allele(2);
	r.add_second_haplotype_allele(1);
	genotypes_chrA[2] = r;

	// generate a GenotypingResult for chrB
	vector<GenotypingResult> genotypes_chrB(2);
	for (size_t i = 0; i < 2; ++i) {
		GenotypingResult r;
		r.add_to_likelihood(0,0,0.1);
		r.add_to_likelihood(0,1,0.1);
		r.add_to_likelihood(1,1,0.8);
		genotypes_chrB[i] = r;
	}
	v.open_genotyping_outfile("../tests/data/small1-genotypes.vcf");
	v.write_genotypes_of("chrA", genotypes_chrA);
	v.write_genotypes_of("chrB", genotypes_chrB);
	v.close_genotyping_outfile();

	v.open_phasing_outfile("../tests/data/small1-phasing.vcf");
	v.write_phasing_of("chrA", genotypes_chrA);
	v.write_phasing_of("chrB", genotypes_chrB);
	v.close_genotyping_outfile();

}

TEST_CASE("VariantReader broken_vcfs", "[VariantReader broken_vcfs]") {
	string no_paths = "../tests/data/no-paths.vcf";
	string malformatted = "../tests/data/malformatted-vcf1.vcf";
	string fasta = "../tests/data/small1.fa";

	CHECK_THROWS(VariantReader(no_paths, fasta, 10));
	CHECK_THROWS(VariantReader(malformatted, fasta, 10));
}
