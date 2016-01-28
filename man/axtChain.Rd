\name{axtChain}
\alias{axtChain}

\title{
  axtChain
}
\description{
  Wrapper function of \emph{axtChain}: chain together psl alignments.
}
\usage{
  axtChain(psls, chains = sub("\\.psl$", ".chain", psls, ignore.case = TRUE),
           assemblyTarget, assemblyQuery,
           distance = c("far", "medium", "far"),
           removePsl = TRUE)
}

\arguments{
  \item{psls}{
    \code{character}(n): file names of input \emph{psl} files.
  }
  \item{chains}{
    code{character}(n): file names of output \emph{chain} files. 
    By default, in the same folder of input lav files with same names.
  }
  \item{assemblyTarget}{
    \code{character}(1): the file name of target assembly \emph{twoBit} file.
  }
  \item{assemblyQuery}{
    \code{character}(1): the file name of query assembly \emph{twoBit} file.
  }
  \item{distance}{
    It can be "far", "medium" or "close". It decides the score matrix used in
    \emph{lastz} aligner. 
    See `?scoringMatrix` for more details.
  }
  \item{removePsl}{
    \code{boolean}: When \code{TRUE}, the input \emph{psl} files will be 
    removed from the conversion.
  }
}

\value{
  \code{character}(n): the file names of output \emph{chain} files.
}
\references{
  \url{http://hgdownload.cse.ucsc.edu/admin/exe/}
}
\author{
  Ge Tan
}

\seealso{
  \code{\link{lavToPsl}}
}
\examples{
  \dontrun{
    psls <- list_files_with_exts(dir="/Users/gtan/Downloads/2bit/axt",
                                 exts="psl")
    assemblyTarget <- "/Users/gtan/Downloads/2bit/fr3.2bit"
    assemblyQuery <- "/Users/gtan/Downloads/2bit/danRer10.2bit"                 
    axtChain(psls, assemblyTarget=assemblyTarget,
             assemblyQuery=assemblyQuery, distance="far",
             removePsl=FALSE)
  }
}