## -----------------------------------------------------------------
## Axt class
## Exported!
setClass(Class="Axt",
         contains="GRangePairs")

setValidity("Axt",
            function(object){
              if(!isConstant(c(length(targetRanges(object)), 
                               length(targetSeqs(object)),
                               length(queryRanges(object)), 
                               length(querySeqs(object)),
                               length(score(object)),
                               length(symCount(object)))))
                return("The lengths of targetRanges, targetSeqs,
                       queryRanges, querySeqs, score and symCount
                       must be same!")
              if(!(identical(width(targetSeqs(object)), symCount(object)) &&
                   identical(width(querySeqs(object)), symCount(object))))
                return("The widths of targetSeqs, querySeqs and 
                       symCount must be same!")
              if(!(all(width(targetRanges(object)) <= symCount(object)) &&
                   all(width(queryRanges(object)) <= symCount(object))))
                return("The widths of targetRanges and queryRanges
                       must be equal or smaller than symCount.")
              ## Test the class
              if(class(targetRanges(object)) != "GRanges")
                return("'x@targetRanges' must be a GRanges instance")
              if(class(queryRanges(object)) != "GRanges")
                return("'x@queryRanges' must be a GRanges instance")
              if(class(targetSeqs(object)) != "DNAStringSet")
                return("'x@targetSeqs' must be a DNAStringSet instance")
              if(class(querySeqs(object)) != "DNAStringSet")
                return("'x@querySeqs' must be a DNAStringSet instance")
              return(TRUE)
            }
                )

### -----------------------------------------------------------------
### Axt Constructor.
### Exported!
Axt <- function(targetRanges=GRanges(), targetSeqs=DNAStringSet(),
                queryRanges=GRanges(), querySeqs=DNAStringSet(),
                score=integer(0), symCount=integer(0),
                names=NULL){
  first <- targetRanges
  first$seqs <- targetSeqs
  last <- queryRanges
  last$seqs <- querySeqs
  new("Axt", NAMES=names, first=first, last=last,
      elementMetadata=DataFrame(score=as.integer(score), 
                                symCount=as.integer(symCount)))
}

### -----------------------------------------------------------------
### Axt class generics
###
setGeneric("targetRanges", function(x) standardGeneric("targetRanges"))
setGeneric("targetSeqs", function(x) standardGeneric("targetSeqs"))
setGeneric("queryRanges", function(x) standardGeneric("queryRanges"))
setGeneric("querySeqs", function(x) standardGeneric("querySeqs"))
setGeneric("symCount", function(x) standardGeneric("symCount"))
setGeneric("subAxt", function(x, chr, start, end,
                              select=c("target", "query"),
                              qSize=NULL) 
  standardGeneric("subAxt")
)
setGeneric("matchDistr", function(x) standardGeneric("matchDistr"))


### - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
### Axt Slot getters and setters.
### Exported!
setMethod("targetRanges", "Axt", function(x) first(x))
setMethod("targetSeqs", "Axt", function(x) first(x)$seqs)
setMethod("queryRanges", "Axt", function(x) last(x))
setMethod("querySeqs", "Axt", function(x) last(x)$seqs)
setMethod("score", "Axt", function(x) mcols(x)$score)
setMethod("symCount", "Axt", function(x) mcols(x)$symCount)
setMethod("nchar", "Axt", function(x) symCount(x))
setMethod("length", "Axt", function(x) length(targetRanges(x)))

### - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
### Subsetting and combining.
### Now it uses the implementation of parent class: GRangePairs

### - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
### "show" method.
###
### 'x' must be an XString or MaskedXString object.
toSeqSnippet <- function(x, width)
{
  if (width < 7L)
    width <- 7L
  seqlen <- length(x)
  if (seqlen <= width) {
    as.character(x)
  } else {
    w1 <- (width - 2) %/% 2
    w2 <- (width - 3) %/% 2
    paste(as.character(subseq(x, start=1, width=w1)),
          "...",
          as.character(subseq(x, end=seqlen, width=w2)),
          sep="")
  }
}


.axt.show_frame_line <- function(x, i, iW, tNameW, tStartW, tEndW, 
                                 qNameW, qStartW, qEndW, scoreW){
  cat(format(i, width=iW, justify="right"), " ",
      format(as.character(seqnames(targetRanges(x)[i])), 
             width=tNameW, justify="right"), " ",
      format(start(targetRanges(x)[i]), width=tStartW, justify="right"), " ",
      format(end(targetRanges(x)[i]), width=tEndW, justify="right"), " ",
      format(as.character(seqnames(queryRanges(x)[i])), 
             width=qNameW, justify="right"), " ",
      format(start(queryRanges(x)[i]), width=qStartW, justify="right"), " ",
      format(end(queryRanges(x)[i]), width=qEndW, justify="right"), " ",
      format(as.character(strand(queryRanges(x))[i]), 
             width=1, justify="right"), " ",
      format(score(x)[i], width=scoreW, justify="right"), " ",
      sep=""
  )
  cat("\n")
  snippetWidth <- getOption("width")
  seq_snippet <- toSeqSnippet(targetSeqs(x)[[i]], snippetWidth)
  cat(seq_snippet)
  cat("\n")
  seq_snippet <- toSeqSnippet(querySeqs(x)[[i]], snippetWidth)
  cat(seq_snippet)
  cat("\n")
}

showAxt <- function(x, margin="", half_nrow=5L){
  lx <- length(x)
  if(is.null((head_nrow = getOption("showHeadLines"))))
    head_nrow = half_nrow
  if(is.null((tail_nrow = getOption("showTailLines"))))
    tail_nrow = half_nrow
  iW = nchar(as.character(lx))
  if(lx < (2*half_nrow+1L) | (lx < (head_nrow+tail_nrow+1L))) {
    tNameW <- max(nchar(as.character(seqnames(targetRanges(x)))))
    tStartW <- max(nchar(as.character(start(targetRanges(x)))))
    tEndW <- max(nchar(as.character(end(targetRanges(x)))))
    qNameW <- max(nchar(as.character(seqnames(queryRanges(x)))))
    qStartW <- max(nchar(as.character(start(queryRanges(x)))))
    qEndW <- max(nchar(as.character(end(queryRanges(x)))))
    scoreW <- max(nchar(as.character(score(x))))
    for(i in seq_len(lx))
      .axt.show_frame_line(x, i, iW, tNameW, tStartW, tEndW, 
                           qNameW, qStartW, qEndW, scoreW)
  }else{
    tNameW <- max(nchar(as.character(seqnames(targetRanges(x)
               [c(1:head_nrow, (lx-tail_nrow+1L):lx)]))))
    tStartW <- max(nchar(as.character(start(targetRanges(x)
                 [c(1:head_nrow, (lx-tail_nrow+1L):lx)]))))
    tEndW <- max(nchar(as.character(end(targetRanges(x)
               [c(1:head_nrow, (lx-tail_nrow+1L):lx)]))))
    qNameW <- max(nchar(as.character(seqnames(queryRanges(x)
               [c(1:head_nrow, (lx-tail_nrow+1L):lx)]))))
    qStartW <- max(nchar(as.character(start(queryRanges(x)
                [c(1:head_nrow, (lx-tail_nrow+1L):lx)]))))
    qEndW <- max(nchar(as.character(end(queryRanges(x)
               [c(1:head_nrow, (lx-tail_nrow+1L):lx)]))))
    scoreW <- max(nchar(as.character(score(x)
                                     [c(1:head_nrow, (lx-tail_nrow+1L):lx)])))
    if(head_nrow > 0){
      for(i in 1:head_nrow)
        .axt.show_frame_line(x, i, iW, tNameW, tStartW, tEndW, 
                             qNameW, qStartW, qEndW, scoreW)
    }
    cat(format("...", width=iW, justify="right"),
        format("...", width=tNameW, justify="right"),
        format("...", width=tStartW, justify="right"),
        format("...", width=tEndW, justify="right"),
        format("...", width=qNameW, justify="right"),
        format("...", width=qStartW, justify="right"),
        format("...", width=qEndW, justify="right"),
        format("...", width=scoreW, justify="right")
    )
    cat("\n")
    if(tail_nrow > 0){
      for(i in (lx-tail_nrow+1L):lx)
        .axt.show_frame_line(x, i, iW, tNameW, tStartW, tEndW, 
                             qNameW, qStartW, qEndW, scoreW)
    }
  }
}

setMethod("show", "Axt",
          function(object){
            lx <- length(object)
            cat(" A ", class(object), " with ", length(object), " ", 
                ifelse(lx == 1L, "alignment pair", "alignment pairs"), 
                ":\n", sep="")
            if(lx != 0){
              showAxt(object, margin="  ")
            }
          }
)