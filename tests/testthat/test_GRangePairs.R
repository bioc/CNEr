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
  expect_identical(first(grangesPairs1), setNames(first, namesGRangePairs))
  expect_identical(first(grangesPairs2), first)
  ## test last
  expect_identical(last(grangesPairs1), setNames(last, namesGRangePairs))
  expect_identical(last(grangesPairs2), last)
  ## test seqnames
  expect_equivalent(seqnames(grangesPairs1),
                   DataFrame(first=Rle(c("chr1", "chr1", "chr2", "chr3")),
                             last=Rle(c("chr1", "chr10", "chr10", "chr20"))))
  ## test strand
  expect_equivalent(strand(grangesPairs1),
                    DataFrame(first=Rle(rep("+", 4)),
                              last=Rle(rep("+", 4))))
  }
          )

