#include "dada.h"
#include <Rcpp.h>
#define OMEGA_A 0.01

using namespace Rcpp;
//' @useDynLib dadac
//' @importFrom Rcpp evalCpp

B *run_dada(Uniques *uniques, double score[4][4], double err[4][4], double gap_pen, bool use_kmers, double kdist_cutoff);
void test_dada(Uniques *uniques, double score[4][4], double err[4][4], double gap_pen, bool use_kmers, double kdist_cutoff);

//------------------------------------------------------------------
//' Run DADA on the provided unique sequences/abundance pairs. 
//'
//' @param seqs (Required). Character.
//'  A vector containing all unique sequences in the data set.
//'  Only A/C/G/T/N/- allowed. Ungapped sequences recommended.
//' 
//' @param abundances (Required). Numeric.
//'  A vector of the number of reads of each unique seuqences.
//'  NAs not tolerated. Must be same length as the seqs vector.
//'
//' @param err (Required). Numeric matrix (4x4).
//'
//' @param score (Required). Numeric matrix (4x4).
//' The score matrix used during the alignment.
//'
//' @param gap (Required). A \code{numeric(1)} giving the gap penalty for alignment.
//'
//' @return List.
//'
//' @export
// [[Rcpp::export]]
Rcpp::List dada_uniques(std::vector< std::string > seqs,  std::vector< int > abundances,
                        Rcpp::NumericMatrix err,
                        Rcpp::NumericMatrix score, Rcpp::NumericVector gap,
                        Rcpp::NumericVector use_kmers, Rcpp::NumericVector kdist_cutoff) {
  int i, j, len1, len2, nrow, ncol;
  
  // Load the seqs/abundances into a Uniques struct
  len1 = seqs.size();
  len2 = abundances.size();
  if(len1 != len2) {
    Rcpp::Rcout << "C: Different input lengths:" << len1 << ", " << len2 << "\n";
    return R_NilValue;
  }
  Uniques *uniques = uniques_from_vectors(seqs, abundances);


  // Copy score into a C style array
  nrow = score.nrow();
  ncol = score.ncol();
  if(nrow != 4 || ncol != 4) {
    Rcpp::Rcout << "C: Score matrix malformed:" << nrow << ", " << ncol << "\n";
    return R_NilValue;
  }
  double c_score[4][4];
  for(i=0;i<4;i++) {
    for(j=0;j<4;j++) {
      c_score[i][j] = score(i,j);
    }
  }

  // Copy err into a C style array
  nrow = err.nrow();
  ncol = err.ncol();
  if(nrow != 4 || ncol != 4) {
    Rcpp::Rcout << "C: Error matrix malformed:" << nrow << ", " << ncol << "\n";
    return R_NilValue;
  }
  double c_err[4][4];
  for(i=0;i<4;i++) {
    for(j=0;j<4;j++) {
      c_err[i][j] = err(i,j);
    }
  }
  
  // Copy gap into a C double
  len1 = gap.size();
  if(len1 != 1) {
    Rcpp::Rcout << "C: Gap penalty not length 1:" << len1 << "\n";
    return R_NilValue;
  }
//  double c_gap = as<double>(gap);
  double c_gap = *(gap.begin());
  
  // Copy use kmers into a C++ bool
  len1 = use_kmers.size();
  if(len1 != 1) {
    Rcpp::Rcout << "C: Use_kmers not length 1:" << len1 << "\n";
    return R_NilValue;
  }
//  bool c_use_kmers = as<bool>(use_kmers);
  bool c_use_kmers = *(use_kmers.begin());

  // Copy kdist_cutoff into a C double
  len1 = kdist_cutoff.size();
  if(len1 != 1) {
    Rcpp::Rcout << "C: Kdist cutoff not length 1:" << len1 << "\n";
    return R_NilValue;
  }
//  double c_kdist_cutoff = as<double>(kdist_cutoff);
  double c_kdist_cutoff = *(kdist_cutoff.begin());

  // TESTING diversion
  if(TESTING) {
    test_dada(uniques, c_score, c_err, c_gap, c_use_kmers, c_kdist_cutoff);
    return Rcpp::List::create();
  }
  
  // Run DADA
  B *bb = run_dada(uniques, c_score, c_err, c_gap, c_use_kmers, c_kdist_cutoff);
  uniques_free(uniques);
  
  // Extract output from B object
  char **ostrs = b_get_seqs(bb);
  int *oabs = b_get_abunds(bb);
  int32_t trans[4][4];
  b_get_trans_matrix(bb, trans);
  
  // Convert to R objects and return
  Rcpp::CharacterVector oseqs;
  Rcpp::NumericVector oabunds(bb->nclust);
  for(i=0;i<bb->nclust;i++) {
    oseqs.push_back(std::string(ostrs[i]));
    oabunds[i] = oabs[i];
  }
  Rcpp::IntegerMatrix otrans(4, 4);  // R INTS ARE SIGNED 32 BIT
  for(i=0;i<4;i++) {
    for(j=0;j<4;j++) {
      otrans(i,j) = trans[i][j];
    }
  }
  
  Rcpp::DataFrame df_genotypes = Rcpp::DataFrame::create(_["sequence"] = oseqs, _["abundance"]  = oabunds);
  return Rcpp::List::create(_["genotypes"] = df_genotypes, _["trans"] = otrans);
}

B *run_dada(Uniques *uniques, double score[4][4], double err[4][4], double gap_pen, bool use_kmers, double kdist_cutoff) {
  int newi = -999, round = 1;
//  double SCORE[4][4] = {{5, -4, -4, -4}, {-4, 5, -4, -4}, {-4, -4, 5, -4}, {-4, -4, -4, 5}};
//  double ERR[4][4] = {{0.991, 0.003, 0.003, 0.003}, {0.003, 0.991, 0.003, 0.003}, {0.003, 0.003, 0.991, 0.003}, {0.003, 0.003, 0.003, 0.991}};  
  B *bb;
  bb = b_new(uniques, err, score, gap_pen); // New cluster with all sequences in 1 bi and 1 fam
  b_fam_update(bb);     // Organizes raws into fams, makes fam consensus sequence
  b_p_update(bb);       // Calculates abundance p-value for each fam in its cluster (consensuses)
  b_print(bb);
  
  while(newi) {
    newi = b_bud(bb, OMEGA_A);
    if(tVERBOSE) Rcout << "C: ----------- Round " << round++ << "-----------\n";
    b_consensus_update(bb);
    b_lambda_update(bb, use_kmers, kdist_cutoff);
    b_shuffle(bb);
    b_consensus_update(bb);
    b_fam_update(bb);
    b_p_update(bb);
  }
  return bb;
}

void test_dada(Uniques *uniques, double score[4][4], double err[4][4], double gap_pen, bool use_kmers, double kdist_cutoff) {
  int i, foo, newi=-999;
  int round = 1;
/*  char *seq1; char *seq2;
  Raw *raw1; Raw *raw2;
  Sub *sub;
  char **al;
  seq1 = (char *) malloc(SEQLEN);
  seq2 = (char *) malloc(SEQLEN);
  uniques_sequence(uniques, 1, seq1);
  uniques_sequence(uniques, 2, seq2);
  raw1 = raw_new(seq1, 2);
  raw2 = raw_new(seq2, 2);
  
  Rcpp::Rcout << "Made raws\n";
  al = raw_align(raw1, raw2, score, gap_pen, 1, kdist_cutoff);
  Rcpp::Rcout << "Made align\n";

  align_print(al);
  sub = al2subs(al);
  compute_lambda(sub, 1e-2, err);*/
  
  B *bb;
  bb = b_new(uniques, err, score, gap_pen); // New cluster with all sequences in 1 bi and 1 fam
  b_fam_update(bb);     // Organizes raws into fams, makes fam consensus sequence
  b_p_update(bb);       // Calculates abundance p-value for each fam in its cluster (consensuses)
  b_print(bb);
  
  while(newi) {
    newi = b_bud(bb, OMEGA_A);
    b_consensus_update(bb);
    b_lambda_update(bb, 1, kdist_cutoff);
    b_shuffle(bb);
    b_consensus_update(bb);
    b_fam_update(bb);
    b_p_update(bb);
    printf("-------------%i--------------\n", newi);
  }
}

//------------------------------------------------------------------
//' Generate the kmer-distance and the alignment distance from the
//'   given set of sequences. 
//'
//' @param seqs (Required). Character.
//'  A vector containing all unique sequences in the data set.
//'  Only A/C/G/T/N/- allowed. Ungapped sequences recommended.
//' 
//' @param score (Required). Numeric matrix (4x4).
//' The score matrix used during the alignment.
//'
//' @param gap (Required). A \code{numeric(1)} giving the gap penalty for alignment.
//'
//' @param max_aligns (Required). A \code{numeric(1)} giving the (maximum) number of
//' pairwise alignments to do.
//'
//' @return DataFrame.
//'
//' @export
// [[Rcpp::export]]
Rcpp::DataFrame calibrate_kmers(std::vector< std::string > seqs, Rcpp::NumericMatrix score, Rcpp::NumericVector gap, size_t max_aligns) {
  int i, j, n_iters, stride, minlen, nseqs, len1 = 0, len2 = 0;
  char *seq1, *seq2;
  double c_gap = as<double>(gap);
  double c_score[4][4];
  for(i=0;i<4;i++) {
    for(j=0;j<4;j++) {
      c_score[i][j] = score(i,j);
    }
  }
  nseqs = seqs.size();
  
  // Find the kdist/align-dist for max_aligns sequence comparisons
  if(max_aligns < (nseqs * (nseqs-1)/2)) { // More potential comparisons than max
    double foo = 2 * sqrt((double) max_aligns);
    n_iters = (int) foo + 2; // n_iters * (n_iters-1)/2 > max_aligns
    stride = nseqs/n_iters;
  } else {
    max_aligns = (nseqs * (nseqs-1)/2);
    n_iters = nseqs;
    stride = 1;
  }

  size_t npairs = 0;
  Rcpp::NumericVector adist(max_aligns);
  Rcpp::NumericVector kdist(max_aligns);
  Sub *sub;
  int *kv1;
  int *kv2;

  for(i=0;i<nseqs;i=i+stride) {
    seq1 = intstr(seqs[i].c_str());
    len1 = strlen(seq1);
    kv1 = get_kmer(seq1, KMER_SIZE);
    for(j=i+1;j<nseqs;j=j+stride) {
      seq2 = intstr(seqs[j].c_str());
      len2 = strlen(seq2);
      kv2 = get_kmer(seq2, KMER_SIZE);

      minlen = (len1 < len2 ? len1 : len2);

      sub = al2subs(nwalign_endsfree(seq1, seq2, c_score, c_gap, BAND));
      adist[npairs] = ((double) sub->nsubs)/((double) minlen);
      
      kdist[npairs] = kmer_dist(kv1, len1, kv2, len2, KMER_SIZE);
      npairs++;
      free(kv2);
      free(seq2);
      if(npairs >= max_aligns) { break; }
    }
    free(kv1);
    free(seq1);
    if(npairs >= max_aligns) { break; }
  }
  
  if(npairs != max_aligns) {
    Rcpp::Rcout << "Warning: Failed to reach requested number of alignments.\n";
  }
  return Rcpp::DataFrame::create(_["align"] = adist, _["kmer"] = kdist);
}
