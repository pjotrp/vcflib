;; To use this file to build HEAD of vcflib:
;;
;;   guix build -f guix.scm
;;
;; Cross compile
;;
;;   guix build  --target=riscv64-linux-gnu  -f guix.scm
;;
;; To get a development container (emacs shell will work)
;;
;;   guix shell -C -D -f guix.scm
;;
;; For the tests you need /usr/bin/env. In a container create it with
;;
;;   mkdir -p /usr/bin ; ln -s $GUIX_ENVIRONMENT/bin/env /usr/bin/env
;;
;; or in one go
;;
;;   guix shell -C -D -f guix.scm -- bash --init-file <(echo "mkdir -p /usr/bin && ln -s \$GUIX_ENVIRONMENT/bin/env /usr/bin/env")
;;
;;   cmake  -DCMAKE_BUILD_TYPE=Debug -DOPENMP=OFF -DASAN=ON ..
;;   cmake --build .
;;
;; debug example
;;
;;   env LD_LIBRARY_PATH=$GUIX_ENVIRONMENT/lib gdb --args vcfallelicprimitives -m ../samples/10158243.vcf


(use-modules
  ((guix licenses) #:prefix license:)
  (guix gexp)
  (guix packages)
  (guix git-download)
  (guix build-system cmake)
  (gnu packages algebra)
  (gnu packages autotools)
  (gnu packages base)
  (gnu packages compression)
  (gnu packages bioinformatics)
  (gnu packages build-tools)
  (gnu packages curl)
  (gnu packages gcc)
  (gnu packages gdb)
  (gnu packages haskell-xyz) ; pandoc for help files
  (gnu packages llvm)
  (gnu packages python)
  (gnu packages parallel)
  (gnu packages perl)
  (gnu packages perl6)
  (gnu packages pkg-config)
  (gnu packages ruby)
  (srfi srfi-1)
  (ice-9 popen)
  (ice-9 rdelim))

(define %source-dir (dirname (current-filename)))

(define %git-commit
    (read-string (open-pipe "git show HEAD | head -1 | cut -d ' ' -f 2" OPEN_READ)))

(define-public vcflib-git
  (package
    (name "vcflib-git")
    (version (git-version "1.0.3" "HEAD" %git-commit))
    (source (local-file %source-dir #:recursive? #t))
    (build-system cmake-build-system)
    (inputs
     `(
       ;; ("curl" ,curl) for htslib
       ;; ("autoconf" ,autoconf) ;; for external htslib
       ;; ("automake" ,automake) ;; for external htslib
       ("fastahack" ,fastahack)
       ;; ("gcc" ,gcc-11)    ;; test against latest
       ("gdb" ,gdb)
       ;; ("htslib" ,htslib)
       ;; ("pandoc" ,pandoc) ;; for generation man pages
       ;; ("perl" ,perl)
       ("python" ,python)
       ;; ("ruby" ,ruby) ;; for generating man pages
       ; ("smithwaterman" ,smithwaterman)
       ;; ("tabixpp" ,tabixpp)
       ("bzip2-static" ,bzip2 "static")
       ("xz-static" ,xz "static")
       ("xz" ,xz)
       ("zlib-static" ,zlib "static")
       ("zlib" ,zlib)))
    (native-inputs
     `(("pkg-config" ,pkg-config)))
    (arguments
     `(#:tests? #f))
    (home-page "https://github.com/vcflib/vcflib/")
    (synopsis "Library for parsing and manipulating VCF files")
    (description "Vcflib provides methods to manipulate and interpret
sequence variation as it can be described by VCF.  It is both an API for parsing
and operating on records of genomic variation as it can be described by the VCF
format, and a collection of command-line utilities for executing complex
manipulations on VCF files.")
    (license license:expat)))

vcflib-git
