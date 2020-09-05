(use-modules
  (srfi srfi-1)
  (ice-9 format)
  (ice-9 ftw)
  (ice-9 regex)
  (ice-9 textual-ports)
  (ice-9 pretty-print)
  (oop goops))

(define %start-time (tms:clock (times)))

(define (profile name proc)
  (define t0 (tms:clock (times)))
  (define ret (proc))
  (define t1 (tms:clock (times)))
  (format (current-error-port) "~a: ~as\n" name
          (exact->inexact (/ (- t1 t0) internal-time-units-per-second)))
  ret)

;; https://rosettacode.org/wiki/Flatten_a_list#Scheme
(define (flatten x depth)
  (cond
    ((null? x) '())
    ((not (pair? x)) (list x))
    (else (append (flatten (car x) (- depth 1))
                  (flatten (cdr x) (- depth 1))))))

(define filename-rx (make-regexp "^([0-9]+)([dijkw])([0-9]+)\\.txt\\.gz$"))
(define not-space (char-set-complement (char-set #\space)))
(define %pi (acos -1))

(define (variable->index var)
  (cond
    ((string=? var "d") 0)
    ((string=? var "i") 1)
    ((string=? var "j") 2)
    ((string=? var "k") 3)
    ((string=? var "w") 4)
    (else (throw 'sbn-error "unknown spectrum variable" var))))

(define-class <spectrum-file> ()
  (path #:init-keyword #:path #:accessor spectrum-file-path)
  (station #:init-keyword #:station #:accessor spectrum-file-station)
  (year #:init-keyword #:year #:accessor spectrum-file-year)
  (variable #:init-keyword #:variable #:accessor spectrum-file-variable))

;;(define (spectrum alpha-1 alpha-2 r-1 r-2 density angle sum)
;;  (+ sum
;;     (* density
;;        (/ 1 %pi)
;;        (+ 0.5
;;           (* 0.01 r-1 (cos (- angle alpha-1)))
;;           (* 0.01 r-2 (cos (* 2 (- angle alpha-2))))))))
;;
;;(define (compute-variance arrays)
;;  (define alpha-1 (list-ref arrays 0))
;;  (define alpha-2 (list-ref arrays 1))
;;  (define r-1 (list-ref arrays 2))
;;  (define r-2 (list-ref arrays 3))
;;  (define density (list-ref arrays 4))
;;  (let* ((theta-0 0)
;;         (theta-1 (* 2 %pi))
;;         (n (length alpha-1))
;;         (angles (map (lambda (j) (+ theta-0 (* (- theta-1 theta-0) (/ j n) ))) (iota n))))
;;    (fold spectrum 0 alpha-1 alpha-2 r-1 r-2 density angles)))

(define input-directories (cdr *application-arguments*))
(define files (make-hash-table))
(format (current-error-port) "input-directories ~a\n" input-directories)
(force-output (current-error-port))
(profile
  "scan-dirs"
  (lambda ()
    (for-each
      (lambda (dir)
        (nftw dir
              (lambda (path statinfo flag base level)
                (if (and (eq? flag 'regular)
                         (eq? (stat:type statinfo) 'regular))
                  (let* ((filename (basename path))
                         (match (regexp-exec filename-rx filename)))
                    (if match
                      (let* ((station (match:substring match 1))
                             (variable (match:substring match 2))
                             (year (match:substring match 3))
                             (key (string-append year "/" station))
                             (existing-value (hash-ref files key)))
                        (hash-set! files key
                                   (cons (make <spectrum-file>
                                           #:path path
                                           #:station station
                                           #:year year
                                           #:variable variable)
                                         (if existing-value existing-value '())))))))
                #t)
              'mount 'physical))
      input-directories)
    (hash-for-each
      (lambda (key value)
        (if (not (= (length value) 5))
          (hash-remove! files key)))
      files)))
(define variances
  (map
    (lambda (five-files)
      (define records (make-hash-table))
      ;; read five files
      (for-each
        (lambda (file)
          (profile
            "read-file"
            (lambda ()
              (define variable (spectrum-file-variable file))
              (define lines
                (filter
                  (lambda (line)
                    (not (string-null? (string-trim-both line))))
                  (string-split
                    (string-trim-both (gzip-file->string (spectrum-file-path file)))
                    #\newline)))
              (for-each
                (lambda (s)
                  (let* ((timestamp (substring s 0 16))
                         (data-string (string-tokenize (substring s 16) not-space))
                         (new-array (map string->number data-string))
                         (arrays (hash-ref records timestamp)))
                    (if (not arrays) (set! arrays (make-list 5)))
                    (list-set! arrays (variable->index variable) new-array)
                    (hash-set! records timestamp arrays)))
                (cdr lines))
              (format (current-error-port) "~a: ~a records\n"
                      (spectrum-file-path file)
                      (hash-count (const #t) records)))))
        five-files)
      ;; remove incomplete records
      (hash-for-each
        (lambda (timestamp arrays)
          (define min-length (apply min (map length arrays)))
          (if (or (not (= (length arrays) 5)) (= min-length 0))
            (hash-remove! records timestamp)
            ;;(hash-set! records timestamp (cons min-length arrays))
            ))
        records)
      ;; compute variance
      (profile "variance"
               (lambda ()
                 (hash-map->list
                   (lambda (timestamp arrays)
                     0.0
                     ;;(define min-length (apply min (map length arrays)))
                     ;;(apply compute-variance (cons min-length arrays))
                     )
                   records)))
      ;;(map
      ;;  (lambda (data)
      ;;    (define file (car five-files))
      ;;    (cons (spectrum-file-station file) data))
      ;;  (hash-map->list
      ;;    (lambda (timestamp arrays)
      ;;      (list timestamp (compute-variance arrays)))
      ;;    records))
      )
    (hash-map->list (lambda (key value) value) files)))

(define %end-time (tms:clock (times)))

(call-with-output-file "time.log"
  (lambda (port)
    (format port "~a\n"
            (exact->inexact (/ (- %end-time %start-time) internal-time-units-per-second)))))

(call-with-output-file "nspectra.log"
  (lambda (port) (format port "~a\n" (length (flatten variances 999)))))

(format (current-error-port) "exit\n")
(force-output (current-error-port))
(kernel-exit 0)
