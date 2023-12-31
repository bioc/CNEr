test_that("test_GRangePairs", {
  library(GenomicRanges)
  first <- GRanges(seqnames=c("chr1", "chr1", "chr2", "chr3"),
                   ranges=IRanges(start=c(1, 20, 2, 3),
                                  end=c(10, 25, 10, 10)),
                   strand="+")
  last <- GRanges(seqnames=c("chr1", "chr10", "chr10", "chr20"),
                  ranges=IRanges(start=c(1, 25, 50, 5),
                                 end=c(8, 40, 55, 16)),
                  strand="+")
  namesGRangePairs <- c("a","b","c","d")
  grangesPairs1 <- GRangePairs(first, last, names=namesGRangePairs)
  grangesPairs2 <- GRangePairs(first, last)
    
  # Test the validity
  ## test the wrong names
  expect_error(GRangePairs(first, last, names=c(1,2,3,4)))
  ## test the first object
  expect_error(GRangePairs(c(1,2,3,4), last))
  ## test the last object
  expect_error(GRangePairs(first, c(1,2,3,4)))
  ## test first and last with different size
  expect_error(GRangePairs(first, last[1:3]))
  ## test first/last and names with different length
  expect_error(GRangePairs(first, last, names=c("a","b","c")))
  
  # Test the getters and setters
  ## test names
  expect_identical(names(grangesPairs1), namesGRangePairs)
  expect_identical(names(grangesPairs2), NULL)
  ## test names<-
  grangesPairs3 <- grangesPairs2
  names(grangesPairs3) <- namesGRangePairs
  expect_identical(grangesPairs3, grangesPairs1)
  ## test length
  expect_identical(length(grangesPairs1), 4L)
  ## test first
  expect_identical(first(grangesPairs1), first)
  expect_identical(first(grangesPairs2), first)
  ## test last
  expect_identical(second(grangesPairs1), last)
  expect_identical(second(grangesPairs2), last)
  
  ## test seqnames
  expect_equivalent(seqnames(grangesPairs1),
                   DataFrame(first=Rle(c("chr1", "chr1", "chr2", "chr3")),
                             second=Rle(c("chr1", "chr10", "chr10", "chr20"))))
  ## test strand
  expect_equivalent(strand(grangesPairs1),
                    DataFrame(first=Rle(rep("+", 4)),
                              second=Rle(rep("+", 4))))
  ## test seqinfo
  expect_identical(length(seqinfo(grangesPairs1)), 2L)
  
  # test Vector methods
  expect_identical(length(grangesPairs1[1:2]), 2L)
  
  # test List methods
  expect_identical(length(suppressWarnings(unlist(grangesPairs1))), 8L)
  
  # test coersion
  expect_identical(length(suppressWarnings(grglist(grangesPairs1))), 4L)
  expect_identical(length(suppressWarnings(as(grangesPairs1, "GRangesList"))),
                   4L)
  expect_identical(length(suppressWarnings(as(grangesPairs1, "GRanges"))), 8L)
  
  expect_identical(dim(as.data.frame(grangesPairs1)), c(4L, 11L))
  
  # test combing
  expect_identical(length(c(unname(grangesPairs1), grangesPairs2)), 8L)
  
  # test swapping
  expect_identical(first(swap(grangesPairs1)), second(grangesPairs1))
  
  # test unique
  expect_identical(unique(c(grangesPairs1, grangesPairs1)), grangesPairs1)
  # test reduce
  # firstGRange <- GRanges(seqnames=c("chr1", "chr1", "chr2", "chr2", "chr5"),
  #                  ranges=IRanges(start=c(1, 20, 2, 3, 1),
  #                                 end=c(10, 25, 10, 10, 10)),
  #                  strand="+")
  # lastGRange <- GRanges(seqnames=c("chr15", "chr10", "chr10", "chr10", "chr15"),
  #                 ranges=IRanges(start=c(1, 25, 50, 51, 5),
  #                                end=c(8, 40, 55, 60, 10)),
  #                 strand="+")
  # grangesPairs3 <- GRangePairs(firstGRange, lastGRange)
  
  }
  )

test_that("test_GRangePairs", {
  dbName <- file.path(system.file("extdata", package="CNEr"),
                      "danRer10CNE.sqlite")
  cneGRangePairs <- readCNERangesFromSQLite(dbName=dbName, 
                                            tableName="danRer10_hg38_45_50")
  ans <- plotCNEWidth(cneGRangePairs)
  
  ## test the xmin and pars value
  expect_equal(ans$first$xmin, 117)
  expect_equal(ans$first$pars, 3.976482, tolerance=1e-5)
  expect_equal(ans$second$xmin, 111)
  expect_equal(ans$second$pars, 3.931556, tolerance=1e-5)
}
)
