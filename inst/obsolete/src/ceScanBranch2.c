/* ceScanBranch1.c - scan axt alignment for conserved elements */
#include "CNEr.h"

/********************************************
 *  *** DATA STRUCTURES AND GLOBAL VARIABLES ***
 *   ********************************************/

/* Scoring matrix.
 *  * This will be set by setBpScores() to 1 for matches and 0 for mismatches and gaps. */

#define NR_CHARS 128
typedef int bpScores_t[NR_CHARS][NR_CHARS];
static bpScores_t bpScores;

/* Data structures to represent start and end coordinate pairs.
 * Used to store filters in memory. */

struct range
/* Start and end coordinate pair */
{
  int start;    /* Start (0 based) */
  int end;    /* End (non-inclusive) */
};

struct rangeArray
/* Array of start and end coordinate pairs */
{
  int n;
  struct range *ranges;
};

struct slRange
/* Start and end coordinate pair as linked list item */
{
  struct slRange *next;
  int start;    /* Start (0 based) */
  int end;    /* End (non-inclusive) */
};


/* Data structure used to represent different thresholds and intermediate results for each */

struct slThreshold
{
  struct slThreshold *next;
  int minScore;
  int winSize;
  int ceStart;
  int ceEnd;
  int nrCNE;
  struct slCNE *CNE;
  FILE *outFile;
};

struct slCNE
{
  struct slCNE *next;
  char *tName; // Name of the target sequence.
  int tStart; // The 1-based coordinate
  int tEnd; // The 1-based coordinate
  char *qName; // Name of the query sequence.
  int qStart;
  int qEnd;
  char strand;
  float score;
  char *cigar;
};

struct slAllCNE
{
  struct slAllCNE *next;
  int minScore;
  int winSize;
  struct slCNE *CNE;
};

/*****************
 *** FUNCTIONS ***
 *****************/
void setBpScores(bpScores_t ss)
/* Set scoring matrix to 1 for matches and 0 for mismatches and gaps. */
{
  unsigned int i, j;
  int a, A;
  char validChars[] = "ACGT";

  // printf("%d\n", (int) sizeof(bpScores_t));

  for (i = 0; i < NR_CHARS; ++i)
    for (j = 0; j < NR_CHARS; ++j)
      ss[i][j] = 0;
  for (i = 0; i < sizeof(validChars)-1; ++i) {
    A = validChars[i];
    a = tolower(A);
    ss[A][A] = ss[a][A] = ss[A][a] = ss[a][a] = 1;
  }
}

struct hash *loadIntHash(char *fileName)
/* Read in a file full of name/number lines into a hash keyed
 * by name with number values. Adapted from axtToMaf.c. */
{
  struct lineFile *lf = lineFileOpen(fileName, TRUE);
  char *row[2];
  struct hash *hash = newHash(0);

  while (lineFileRow(lf, row)) {
    int num = lineFileNeedNum(lf, row, 1);
    hashAddInt(hash, row[0], num);
  }

  lineFileClose(&lf);
  return hash;
}

struct hash *readBed(char *fileName)
/* Read a 3-column bed file into a hash, where keys are sequence names
 * and values are linked lists of coordinate ranges (slRange structures). */
{
  struct lineFile *lf = lineFileOpen(fileName, TRUE);
  struct hash *hash = newHash(0);
  struct hashEl *hel;
  struct slRange *range;
  char *row[3];

  while (lineFileRow(lf, row)) {
    if(sameString(row[0], "track") || sameString(row[0], "browser")) continue;
    AllocVar(range);
    range->next = NULL;
    range->start = lineFileNeedNum(lf, row, 1);
    range->end = lineFileNeedNum(lf, row, 2);
    if (range->start > range->end)
      errAbort("start after end line %d of %s", lf->lineIx, lf->fileName);

    hel = hashLookup(hash, row[0]);
    if (hel == NULL)
      hel = hashAdd(hash, row[0], range);
    else {
      slSafeAddHead(&hel->val, range);
    }
  }

  lineFileClose(&lf);

  return hash;
}

int slRangeCmpStart(const void *va, const void *vb)
/* Comparison function to sort linked list of ranges by start coordinate. */
{
  const struct slRange *a = *((struct slRange **)va);
  const struct slRange *b = *((struct slRange **)vb);
  return a->start - b->start;
}

void collapseRangeList(struct hashEl *hel)
/* Collapse a range list to a set of sorted, non-overlapping ranges. */
{
  struct slRange *a, *b;
  slSort(&hel->val, slRangeCmpStart); /* sort by start coord */
  a = hel->val;
  while((b = a->next)) {
    if(b->start <= a->end) {
      if(a->end < b->end) a->end = b->end;
      a->next = b->next;
      freez(&b);
    }
    else a = b;
  }
  /*for(a = hel->val; a; a=a->next) {
    printf("%d\t%d\n", a->start, a->end);
    }*/
}

void convertRangeListToArray(struct hashEl *hel)
/* Convert a linked list of ranges to an array.
 * The reason for doing this is that we can do a fast binary search on the array. */
{
  struct slRange *list, *slEl;
  struct range *arrayEl;
  struct rangeArray *arrayInfo;
  int n;

  list = hel->val;
  n = slCount(list)+1;
  AllocVar(arrayInfo);
  arrayInfo->n = n;
  arrayInfo->ranges = arrayEl = needMem(n * sizeof(*arrayEl));
  hel->val = arrayInfo;

  while((slEl = slPopHead(&list))) {
    arrayEl->start = slEl->start;
    arrayEl->end = slEl->end;
    free(slEl);
    arrayEl++;
  }

  /* The last array element is a "dummy" element that contains a coordinate pair
   * beyond any chromosome size. The presence of this element simplifies going
   * through the array in scanAxt() as it removes the need for an out-of-bounds check. */
  arrayEl->start = 1e9;
  arrayEl->end = 1e9+1;
}

void printRangeArray(struct hashEl *hel)
/* Print a range array. For debugging purposes only. */
{
  struct rangeArray *arrayInfo = hel->val;
  struct range *ranges = arrayInfo->ranges;
  int i;
  printf("%s n=%d\n", hel->name, arrayInfo->n);
  for(i = 0; i < arrayInfo->n; i++) {
    printf("%02d: %d - %d\n", i, ranges[i].start, ranges[i].end);
  }
}

struct range *searchRangeArray(struct rangeArray *arrayInfo, int key)
/* Binary search range array. */
{
  struct range *array = arrayInfo->ranges;
  int low = 0;
  int high = arrayInfo->n - 1;
  int mid;

  while(low <= high) {
    mid = (low+high)/2;
    if(key <= array[mid].start) high = mid - 1;
    else if(key > array[mid].end) low = mid + 1;
    else return array+mid; /* return pointer to range that contains key */
  }

  /* key not found: return pointer to nearest higher range or abort if there is no higher range
   * (there should be one because we have added a dummy range with very high values) */
  if(low >= arrayInfo->n) errAbort("searchRangeArray: key %d out of bounds\n", key);
  return array+low;
}

struct hash *readFilter(char *fileName)
/* Load a filter file. */
{
  struct hash *hash = readBed(fileName);
  hashTraverseEls(hash, collapseRangeList);
  hashTraverseEls(hash, convertRangeListToArray);
  /* hashTraverseEls(hash, printRangeArray); */
  return hash;
}

struct hash *makeReversedFilter(struct hash *f1, struct hash *chrSizes)
/* Given a filter, create a reversed filter where coordinates increase in the opposite direction.
 * We use this for filtering alignments that have qStrand == '-'. */
  // This built hash can be released by freeHashAndValsForRanges
  // Then this function has no memory leak now!
{
  struct hash *f2 = newHash(0);
  struct hashCookie cookie = hashFirst(f1);
  struct hashEl *hel;
  struct rangeArray *fwd, *rev;
  struct range *arrayEl;
  int i, j, n, chrSize;

  /* Iterate over all sequences (chromosomes) in filter */
  while((hel = hashNext(&cookie))) {

    /* get sequence size */
    chrSize = hashIntVal(chrSizes, hel->name);

    /* get forward range array */
    fwd = hel->val;

    /* allocate memory for reversed range array */
    AllocVar(rev);
    n = rev->n = fwd->n; /* set nr of elements in range */
    rev->ranges = arrayEl = needMem(n * sizeof(struct range));

    /* copy dummy range */
    rev->ranges[n-1] = fwd->ranges[n-1];

    /* reverse other ranges */
    for(i = 0, j = n-2; j >= 0; i++, j--) {
      rev->ranges[i].start = chrSize - fwd->ranges[j].end;
      rev->ranges[i].end = chrSize - fwd->ranges[j].start;
    }

    /* add range array to hash keyed by sequence name */
    hashAdd(f2, hel->name, rev);
  }

  /* return reverse filter */
  return f2;
}

struct range *searchFilter(struct hash *filter, char *chrom, int pos)
/* Find the first filter at or following a given position */
{
  struct hashEl *hel;

  hel = hashLookup(filter, chrom);   /* find range array for sequence (chromosome) */
  if(hel) return searchRangeArray(hel->val, pos); /* search range array by position */
  else return NULL;
}

void printCigarString(FILE *fh, struct axt *axt, int i, int j)
/* Print CIGAR string that summarizes alignment */
{
  char type = 'M'; /* in our case first column is always match */
  char newType;
  int count = 0;

  for(; i <= j; i++) {
    /* Determine column type */
    if(axt->tSym[i] == '-') newType = 'D';
    else if(axt->qSym[i] == '-') newType = 'I';
    else newType = 'M';
    /* If same type as previous, just increase count, otherwise output previous */
    if(type == newType) count++;
    else {
      fprintf(fh, "%d%c", count, type);
      type = newType;
      count = 1;
    }
  }

  if(count) fprintf(fh, "%d%c", count, type);
}

void addCigarString(struct slCNE *CNE, struct axt *axt, int i, int j){
  /* Add cigar string to CNE object*/
  char type = 'M'; /* in our case first column is always match */
  char newType;
  int count = 0;
  // This is potentially risky to limit the cigar string to 1000 length long. Use realloc() to replace it later.
  char temp[100];
  char cigar[1000];
  strcpy(cigar, "");
  for(; i <= j; i++) {
    /* Determine column type */
    if(axt->tSym[i] == '-') newType = 'D';
    else if(axt->qSym[i] == '-') newType = 'I';
    else newType = 'M';
    /* If same type as previous, just increase count, otherwise output previous */
    if(type == newType) 
      count++;
    else{
      sprintf(temp, "%d%c", count, type);
      strcat(cigar, temp);
      type = newType;
      count = 1;
    }
  }
  if(count){
    sprintf(temp, "%d%c", count, type);
    strcat(cigar, temp);
  }
  char *holdCigar = (char *) malloc(sizeof(char) * 1000);
  //char *holdCigar = (char *) R_alloc(1000, sizeof(char));
  strcpy(holdCigar, cigar); 
  CNE->cigar = holdCigar;
}


void printElement(struct slThreshold *tr, struct axt *axt, struct hash *qSizes, int *profile, int *tPosList, int *qPosList)
/* Print one conserved element on stdout.
 * Arguments:
 * tr - contains threshold-specific information:
 *      parameters used to find CE, CE location in alignment, and filehandle to print to
 * axt - alignment
 * qSizes - query assembly chromosome sizes
 * profile - cumulative conservation profile for alignment
 * tPosList, qPosList - target and query position arrays for alignment
 */
{
  int score, qStart, qEnd, qSize;
  int i = tr->ceStart; /* start column of conserved element in alignment */
  int j = tr->ceEnd; /* end column of conserved element in alignment */

  /* truncate edges (mismatches and gaps) */
  while(bpScores[ (int) axt->qSym[i] ][ (int) axt->tSym[i] ] <= 0) i++;
  while(bpScores[ (int) axt->qSym[j] ][ (int) axt->tSym[j] ] <= 0) j--;

  /* compute score */
  score = profile[j] - profile[i] + bpScores[ (int) axt->qSym[i] ][ (int) axt->tSym[i] ];

  /* recompute query positions if query strand is - */
  if(axt->qStrand == '+') {
    qStart = qPosList[i];
    qEnd = qPosList[j];
  }
  else {
    qSize = hashIntVal(qSizes, axt->qName);
    qStart = qSize - qPosList[j] + 1;
    qEnd = qSize - qPosList[i] + 1;
  }

  /* output */
  fprintf(tr->outFile, "%s\t%d\t%d\t%s\t%d\t%d\t%c\t%.2f\t",
    axt->tName, tPosList[i]-1, tPosList[j],
    axt->qName, qStart-1, qEnd,
    axt->qStrand, 100.0 * score / (j-i+1));
  printCigarString(tr->outFile, axt, i, j);
  fputs("\n", tr->outFile);
}

void addCNE(struct slThreshold *tr, struct axt *axt, struct hash *qSizes, int *profile, int *tPosList, int *qPosList){
  /* add one cne to slThreshold object's CNE element
   * Arguments:
   * tr - contains threshold-specific information:
   * parameters used to find CE, CE location in alignment, and filehandle to print to
   * axt - alignment
   * qSizes - query assembly chromosome sizes
   * profile - cumulative conservation profile for alignment
   * tPosList, qPosList - target and query position arrays for alignment
   */
  int score, qStart, qEnd, qSize;
  struct slCNE *CNE=NULL;
  int i = tr->ceStart; /* start column of conserved element in alignment */
  int j = tr->ceEnd; /* end column of conserved element in alignment */
   /* truncate edges (mismatches and gaps) */
  while(bpScores[ (int) axt->qSym[i] ][ (int) axt->tSym[i] ] <= 0) i++;
  while(bpScores[ (int) axt->qSym[j] ][ (int) axt->tSym[j] ] <= 0) j--;

  /* compute score */
  score = profile[j] - profile[i] + bpScores[ (int) axt->qSym[i] ][ (int) axt->tSym[i] ];

  /* recompute query positions if query strand is - */
  if(axt->qStrand == '+') {
    qStart = qPosList[i];
    qEnd = qPosList[j];
  }
  else {
    qSize = hashIntVal(qSizes, axt->qName);
    qStart = qSize - qPosList[j] + 1;
    qEnd = qSize - qPosList[i] + 1;
  }
  
  /* add one cne to slThreshold object's CNE element */
  tr->nrCNE++; // record the number of CNEs
  CNE = needMem(sizeof(*CNE));
  CNE->tName = axt->tName;
  CNE->tStart = tPosList[i]-1;
  CNE->tEnd = tPosList[j];
  CNE->qName = axt->qName;
  CNE->qStart = qStart-1;
  CNE->qEnd = qEnd;
  CNE->strand = axt->qStrand;
  CNE->score = 100.0 * score / (j-i+1);
  addCigarString(CNE, axt, i, j);
  slAddHead(&(tr->CNE), CNE);
  //free(CNE->cigar);
  //freez(&CNE);
}


void scanAxt(struct axt *axt, struct hash *qSizes, struct hash *tFilterAll, struct hash *qFilterAll, struct slThreshold *thresholds)
/* Scan one axt alignment and print conserved elements found to stdout.
 * THIS IS THE CORE FUNCTION OF THIS PROGRAM.
 * Arguments:
 * axt - alignment
 * qSizes - query assembly chromosome sizes
 * tFilterAll, qFilterAll - index of regions to exclude from scan
 * winSize - size of sliding window
 * thresholds - linked list of thresholds to call CEs at, and corresponding output filehandles
 */
{
  /* Variables to keep track of things as we loop through the alignment */
  int i = 0; /* column in alignment */
  int tPos = axt->tStart; /* position in target sequence */
  int qPos = axt->qStart; /* position in query sequence */
  int nrColumns;  /* counter for nr of columns seen after mask */
  int score;    /* sliding window score */
  struct slThreshold *tr;

  /* Three arrays where each element corresponds to a column in the alignment. */
  int *profile = needLargeMem(axt->symCount * sizeof(int)); /* cumulative conservation profile */
  int *tPosList = needLargeMem(axt->symCount * sizeof(int)); /* target seq position */
  int *qPosList = needLargeMem(axt->symCount * sizeof(int)); /* query seq position */
  /* E.g. at alignment column 5, target position tPosList[4] is aligned with query position qPosList[4],
   *      and alignment columns 1-5 contain a total of profile[4] matches.
   * Note:
   *  - in these 3 arrays, elements that that correspond to masked regions are not set
   *  - profile[] begins from zero again after each mask
   *  - target and query positions are set to -1 at gaps. */

  /* tFilter and qFilter are pointers to sorted arrays of coordinate ranges that should not be scanned.
   * The calls to searchFilter find the first filter overlapping or following the alignment */
  struct range *tFilter = tFilterAll != NULL ? searchFilter(tFilterAll, axt->tName, axt->tStart+1) : NULL;
  struct range *qFilter = qFilterAll != NULL ? searchFilter(qFilterAll, axt->qName, axt->qStart+1) : NULL;
  /* Initialize CE bounds for each threshold */
  for(tr = thresholds; tr != NULL; tr = tr->next) {
    tr->ceStart = -1; /* set to -1 = no CE found */
  }
  
  /* Main loop: go through alignment */
  while(i < axt->symCount) { /* loop until we have looked at entire alignment */

    /* if inside a mask, fast forward past it */
    do {
      if(tFilter != NULL) {
  while(tFilter->end <= tPos) tFilter++;
  if(tFilter->start <= tPos) {
    if(tFilter->end >= axt->tEnd) goto endScan; /* using goto to break out of nested loop */
    while(tFilter->end > tPos) {
      if(axt->tSym[i] != '-') tPos++;
      if(axt->qSym[i] != '-') qPos++;
      i++;
    }
    tFilter++;
  }
      }
      if(qFilter != NULL) {
  while(qFilter->end <= qPos) qFilter++;
  if(qFilter->start <= qPos) {
    if(qFilter->end >= axt->qEnd) goto endScan; /* using goto to break out of nested loop */
    while(qFilter->end > qPos) {
      if(axt->tSym[i] != '-') tPos++;
      if(axt->qSym[i] != '-') qPos++;
      i++;
    }
    qFilter++;
  }
      }
    } while(tFilter != NULL && tFilter->start <= tPos);

    /* handle first position after mask */
    profile[i] = bpScores[ (int) axt->qSym[i] ][ (int) axt->tSym[i] ];
    tPosList[i] = axt->tSym[i] == '-' ? -1 : ++tPos;
    qPosList[i] = axt->qSym[i] == '-' ? -1 : ++qPos;
    nrColumns = 1;

    /* handle remaining positions */
    for(i++; i < axt->symCount; i++) {
      /* break out of loop if we have come to a mask */
      if((tFilter != NULL && tFilter->start <= tPos) || (qFilter != NULL && qFilter->start <= qPos)) break;
      /* set positions */
      tPosList[i] = axt->tSym[i] == '-' ? -1 : ++tPos;
      qPosList[i] = axt->qSym[i] == '-' ? -1 : ++qPos;
      /* set profile */
      profile[i] = profile[i-1] + bpScores[ (int) axt->qSym[i]][ (int) axt->tSym[i] ];
      /* increment nr of columns seen after mask */
      nrColumns++;
      /* loop over user-defined thresholds */
      for(tr = thresholds; tr != NULL; tr = tr->next) {
      /* if have have seen enough columns to cover a window, evaluate that window */
        if(nrColumns >= tr->winSize) {
        /* compute and check window score */
          score = nrColumns > tr->winSize ? profile[i] - profile[i - tr->winSize] : profile[i];
          if(score >= tr->minScore) {
          /* score is above threshold: begin or extend conserved element */
            if(tr->ceStart == -1) tr->ceStart = i - tr->winSize + 1;
              tr->ceEnd = i;
          }
          else {
          /* score is below threshold: close and print any open conserved elements that are more than a window away */
            if(tr->ceStart != -1 && tr->ceEnd < i - tr->winSize + 1) {
              printElement(tr, axt, qSizes, profile, tPosList, qPosList);
              //addCNE(tr, axt, qSizes, profile, tPosList, qPosList);
              tr->ceStart = -1;
            }
          }
        }
      }
    }

    /* close and print any open conserved elements */
    for(tr = thresholds; tr != NULL; tr = tr->next) {
      if(tr->ceStart != -1) {
        printElement(tr, axt, qSizes, profile, tPosList, qPosList);
        //addCNE(tr, axt, qSizes, profile, tPosList, qPosList);
        tr->ceStart = -1;
      }
    }
  }

 endScan:

  /* free memory */
  freez(&profile);
  freez(&tPosList);
  freez(&qPosList);
}

void ceScan1(char *tFilterFile, char *qFilterFile, char *qSizeFile, struct slThreshold *thresholds, int nrAxtFiles, char *axtFiles[])
/* ceScan - Find conserved elements. */
{
  struct lineFile *lf;
  struct axt *axt;
  struct hash *tFilter, *qFilter, *qFilterRev, *qSizes;
  int i;
  
  setBpScores(bpScores);
  qSizes = loadIntHash(qSizeFile);
  tFilter = tFilterFile ? readFilter(tFilterFile) : NULL;
  qFilter = qFilterFile ? readFilter(qFilterFile) : NULL;
  qFilterRev = qFilter ? makeReversedFilter(qFilter, qSizes) : NULL;

  //i = 0;
  for(i = 0; i < nrAxtFiles; i++) {
    lf = lineFileOpen(axtFiles[i], TRUE);
    while ((axt = axtRead(lf)) != NULL) {
      scanAxt(axt,
        qSizes,
        tFilter,
        axt->qStrand == '+' ? qFilter : qFilterRev,
        thresholds);
      axtFree(&axt);
    }
    lineFileClose(&lf);
  }
}

void ceScan(char **tFilterFile, char **qFilterFile, char **qSizeFile, int *winSize, int *minScore, int *nThresholds, char **axtFiles, int *nrAxtFiles, char **outFilePrefix){
  int i, n;
  struct slThreshold *trList = NULL, *tr;
  char rest, path[PATH_LEN];
  for(i=1; i<=*nThresholds;i++)
  {
    tr = needMem(sizeof(*tr));
    tr->minScore = *minScore++;
    tr->winSize = *winSize++;
    safef(path, sizeof(path), "%s_%d_%d", *outFilePrefix, tr->minScore, tr->winSize);
    tr->outFile = mustOpen(path, "w");
    slAddHead(&trList, tr);
  }
  /* Call function ceScan with the arguments */
  ceScan1(*tFilterFile, *qFilterFile, *qSizeFile, trList, *nrAxtFiles, axtFiles);
  /* Close all output files */
  for(tr = trList; tr != NULL; tr = tr->next)
    fclose(tr->outFile);
}

/*######################################################*/

void freeRangeArray(struct hashEl *hel)
{
  struct rangeArray *arrayInfo;
  arrayInfo = hel->val;
  free(arrayInfo->ranges);
  free(arrayInfo);
}

void freeHashAndValsForRanges(struct hash **pHash)
/* Free up hash table and all values associated with it.
 * (Just calls freeMem on each hel->val) */
{
struct hash *hash;
if ((hash = *pHash) != NULL)
    {
    hashTraverseEls(hash, freeRangeArray);
    freeHash(pHash);
    }
}

void freeAxtListOnly(struct axt **pList)
{
  struct axt *el, *next;
  for(el = *pList; el != NULL; el = next)
  {
    next = el->next;
    freez(&el);
  }
  *pList = NULL;
}

void freeSlThreshold(struct slThreshold **p_thresholds)
// Free up a SlThreshold class and the CNEs inside
{
  struct slThreshold *thresholds, *nextThreshold, *el_threshold;
  struct slCNE *CNE, *nextCNE, *el_CNE;
  nextThreshold = *p_thresholds;
  while(nextThreshold != NULL)
  {
    //Rprintf("I am in free Slthreshold\n");
    el_threshold = nextThreshold;
    nextCNE = el_threshold->CNE;
    while(nextCNE != NULL){
      // Rprintf("I am in free CNE\n");
      el_CNE = nextCNE;
      nextCNE = el_CNE->next;
      free(el_CNE->cigar);
      freez(&el_CNE);
    }
    nextThreshold = el_threshold->next;
    freez(&el_threshold);
  }
  *p_thresholds = NULL;
}

struct hash *buildHashForBed(SEXP tNames, SEXP tStarts, SEXP tEnds){
/* Given three vectors of names, starts and ends of the filter, return the hash table */
  // Here the tStarts are in 1-based coordinate. In the hash, it's in 0-based.
  // The built hash can be released by the freeHashAndValsForRanges.
  tNames = AS_CHARACTER(tNames);
  tStarts = AS_INTEGER(tStarts);
  tEnds = AS_INTEGER(tEnds);
  struct hash *hash = newHash(0);
  struct slRange *range;
  struct hashEl *hel;
  int i, n, *p_tStarts, *p_tEnds;
  p_tStarts = INTEGER_POINTER(tStarts);
  p_tEnds = INTEGER_POINTER(tEnds);
  n = GET_LENGTH(tNames);
  if(n == 0){
    return NULL;
  }
  for(i = 0; i < n ; i++){
    AllocVar(range);
    range->next = NULL;
    range->start = p_tStarts[i] - 1;
    range->end = p_tEnds[i];
    char *tName = (char *) malloc(sizeof(char) * strlen(CHAR(STRING_ELT(tNames, i))));
    strcpy(tName, CHAR(STRING_ELT(tNames, i)));
    hel = hashLookup(hash, tName);
    if(hel == NULL)
      hel = hashAdd(hash, tName, range);
    else
      slSafeAddHead(&hel->val, range);
    free(tName);
   // freez(&range);
  }
  hashTraverseEls(hash, collapseRangeList);
  hashTraverseEls(hash, convertRangeListToArray);
  return hash;
}

struct hash *buildHashForSizeFile(SEXP names, SEXP sizes){
  // There is no memory leak for this function now!
  //This built hash can be released by the freeHash.
  names = AS_CHARACTER(names);
  sizes = AS_INTEGER(sizes);
  struct hash *hash = newHash(0);
  int i, *p_sizes, n = GET_LENGTH(names);
  p_sizes = INTEGER_POINTER(sizes);
  for(i = 0; i < n; i++){
    char *name = (char *) malloc(sizeof(char) * strlen(CHAR(STRING_ELT(names, i))));
    strcpy(name, CHAR(STRING_ELT(names, i)));
    hashAddInt(hash, name, p_sizes[i]);
    free(name);
  }
  return hash;
}

struct axt *buildAxt(SEXP axtqNames, SEXP axtqStart, SEXP axtqEnd, SEXP axtqStrand, SEXP axtqSym, SEXP axttNames, SEXP axttStart, SEXP axttEnd, SEXP axttStrand, SEXP axttSym, SEXP score, SEXP symCount){
  // The built axt can be freed by axtFreeList
  axtqNames = AS_CHARACTER(axtqNames);
  axtqStart = AS_INTEGER(axtqStart);
  axtqEnd = AS_INTEGER(axtqEnd);
  axtqStrand = AS_CHARACTER(axtqStrand);
  axtqSym = AS_CHARACTER(axtqSym);
  axttNames = AS_CHARACTER(axttNames);
  axttStart = AS_INTEGER(axttStart);
  axttEnd = AS_INTEGER(axttEnd);
  axttStrand = AS_CHARACTER(axttStrand);
  axttSym = AS_CHARACTER(axttSym);
  score = AS_INTEGER(score);
  symCount = AS_INTEGER(symCount);
  int i, *p_axtqStart, *p_axtqEnd, *p_axttStart, *p_axttEnd, *p_score, *p_symCount;
  p_axtqStart = INTEGER_POINTER(axtqStart);
  p_axtqEnd = INTEGER_POINTER(axtqEnd);
  p_axttStart = INTEGER_POINTER(axttStart);
  p_axttEnd = INTEGER_POINTER(axttEnd);
  p_score = INTEGER_POINTER(score);
  p_symCount = INTEGER_POINTER(symCount);

  struct axt *axt=NULL, *curAxt;
  int nrAxt = GET_LENGTH(axtqNames);
  for(i = 0; i < nrAxt; i++){
    AllocVar(curAxt);
    //This will cause the warning during compilation, but can save time. No need to create a none const char for it.
    curAxt->qName = CHAR(STRING_ELT(axtqNames, i));
    //Make it back to 0-based coordinates for start
    curAxt->qStart = p_axtqStart[i] - 1;
    curAxt->qEnd = p_axtqEnd[i];
    curAxt->qStrand = CHAR(STRING_ELT(axtqStrand, i))[0];
    curAxt->qSym = CHAR(STRING_ELT(axtqSym, i));
    curAxt->tName = CHAR(STRING_ELT(axttNames, i));
    curAxt->tStart = p_axttStart[i] - 1;
    curAxt->tEnd = p_axttEnd[i];
    curAxt->tStrand = CHAR(STRING_ELT(axttStrand, i))[0];
    curAxt->tSym = CHAR(STRING_ELT(axttSym, i));
    curAxt->score = p_score[i];
    curAxt->symCount = p_symCount[i];
    curAxt->next = axt;
    axt = curAxt;
  }
  //axtFree(curAxt);
  //UNPROTECT(12);
  return axt;
}

struct slThreshold *buildThreshold(SEXP winSize, SEXP minScore, SEXP outputFiles){
  // checked for memory leak!
  // The built slThreshold should be freed by slFreeList 
  struct slThreshold *trList = NULL, *tr;
  char path[PATH_LEN];
  winSize = AS_INTEGER(winSize);
  minScore = AS_INTEGER(minScore);
  int i, nThresholds = GET_LENGTH(winSize);
  int *p_winSize, *p_minScore;
  p_winSize = INTEGER_POINTER(winSize);
  p_minScore = INTEGER_POINTER(minScore);
  for(i = 0; i < nThresholds; i++){
    tr = needMem(sizeof(*tr));
    tr->minScore = p_minScore[i];
    tr->winSize = p_winSize[i];
    //safef(path, sizeof(path), "%s_%d_%d", CHAR(STRING_ELT(outFilePrefix, 0)), tr->minScore, tr->winSize);
    char *filepath_elt = (char *) R_alloc(strlen(CHAR(STRING_ELT(outputFiles, i))), sizeof(char));
    strcpy(filepath_elt, CHAR(STRING_ELT(outputFiles, i)));
    //Rprintf("The output file is %s\n", filepath_elt);
    tr->outFile = mustOpen(filepath_elt, "w");
    slAddHead(&trList, tr);
  }
  return trList;
}

SEXP myCeScan(SEXP tFilterNames, SEXP tFilterStarts, SEXP tFilterEnds, SEXP qFilterNames, SEXP qFilterStarts, SEXP qFilterEnds, SEXP sizeNames, SEXP sizeSizes, SEXP axttNames, SEXP axttStart, SEXP axttEnd, SEXP axttStrand, SEXP axttSym, SEXP axtqNames, SEXP axtqStart, SEXP axtqEnd, SEXP axtqStrand, SEXP axtqSym, SEXP score, SEXP symCount, SEXP winSize, SEXP minScore, SEXP outputFiles){
  struct hash *tFilter, *qFilter, *qFilterRev, *qSizes;
  struct axt *axt, *curAxt;
  tFilter = buildHashForBed(tFilterNames, tFilterStarts, tFilterEnds);
  qFilter = buildHashForBed(qFilterNames, qFilterStarts, qFilterEnds);
  qSizes = buildHashForSizeFile(sizeNames, sizeSizes); 
  qFilterRev = qFilter ? makeReversedFilter(qFilter, qSizes) : NULL;
  axt = buildAxt(axtqNames, axtqStart, axtqEnd, axtqStrand, axtqSym, axttNames, axttStart, axttEnd, axttStrand, axttSym, score, symCount);
  // here I decided to build axt in the linked axt, rather than one by one. Perhaps it has lower performance than one by one way.
  struct slThreshold *thresholds, *tr, *curThresholds;
  struct slCNE *CNE;
  //struct hashEl *hel;
  int nrThresholds;
  nrThresholds = GET_LENGTH(winSize);
  int i;
  thresholds = buildThreshold(winSize, minScore, outputFiles);
  setBpScores(bpScores);
  //SEXP tName, tStart, tEnd, qName, qStart, qEnd, strand, CNEscore, cigar, returnList, oneList, list_names, returnListNames, cigarWidth; 
  //int k = 0;
  curAxt = axt;
  while(curAxt){
    //if(qSizes != NULL){
    //  hel = hashLookup(qSizes, curAxt->qName);
    //    if(hel == NULL)
    //      continue;
    //}
    scanAxt(curAxt, qSizes, tFilter, axt->qStrand == '+' ? qFilter : qFilterRev, thresholds);
    curAxt = curAxt->next;
    //if(k > 50) break;
    //k++;
  }
  for(tr = thresholds; tr != NULL; tr = tr->next)
    fclose(tr->outFile);
  freeHashAndValsForRanges(&tFilter);
  freeHashAndValsForRanges(&qFilter);
  freeHash(&qSizes);
  freeHashAndValsForRanges(&qFilterRev);
  freeAxtListOnly(&axt);
  freeSlThreshold(&thresholds);
  return R_NilValue;
  //return returnList;
}

