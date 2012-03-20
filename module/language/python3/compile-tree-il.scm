;;; Python 3 for Guile

;; Copyright (C) 2012 Stefan Kangas.
;; Copyright (C) 2012 Per Reimers.
;; Copyright (C) 2012 David Spångberg.
;; Copyright (C) 2012 Krister Svanlund.

;;;; This library is free software; you can redistribute it and/or
;;;; modify it under the terms of the GNU Lesser General Public
;;;; License as published by the Free Software Foundation; either
;;;; version 3 of the License, or (at your option) any later version.
;;;;
;;;; This library is distributed in the hope that it will be useful,
;;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;;;; Lesser General Public License for more details.
;;;;
;;;; You should have received a copy of the GNU Lesser General Public
;;;; License along with this library; if not, write to the Free Software
;;;; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

;;; Code:

(define-module (language python3 compile-tree-il)
  #:use-module (language python3 commons)
  #:use-module (language python3 impl)
  #:use-module (language tree-il)
  #:use-module (system base pmatch)
  #:use-module (srfi srfi-1)
  #:export (compile-tree-il test))

;; Syntax-rules stolen from (language ecmascript compile-tree-il) :)
(define-syntax-rule (-> (type arg ...))
  `(type ,arg ...))

(define-syntax-rule (@implv sym)
  (-> (@ '(language python3 impl) 'sym)))

(define-syntax-rule (@impl sym arg ...)
  (-> (call (@implv sym) arg ...)))

(define (econs name gensym env)
  (acons name gensym env))

(define (lookup name env)
  "Looks up a given name in a given environment."
  (assq-ref env name))

(define (compile-tree-il exp env opts)
    "Compiles a given python3 expressions in a given environment into a
corresponding tree-il expression."
  (let ((ret (comp exp '())))
    (values
     (parse-tree-il ret)
     env
     env)))

(define (comp x e)
  "Compiles a given python3 expressions in a given environment into a
corresponding tree-il expression."
  (pmatch x
    ;; module code
    ((<module> ,stmts)
     (comp-block #t stmts e))

    ;; stmt code
    ((<function-def> ,id ,args ,body ,decos ,ret)
     `(define ,id
        (lambda ()
          ,(comp-fun-body id args body e))))
    ((<return> ,exp)
     `(primcall return ,(comp exp e)))
    ((<assign> ,targets ,value)
     (pmatch targets
       (((<name> ,name <store>))
        `(define ,name ,(comp value e)))
       (((<tuple> ,names))
        ;; tuples, lists and starred not implemented
        #f)))
    (<pass>
     '(void))
    ((<expr> ,exp)
     (comp exp e))

    ;; expressions
    ((<bin-op> ,eleft ,op ,eright)
     (comp-bin-op op eleft eright e))
    ((<bool-op> ,op ,lst)
     (comp-bool-op op lst e))
    ((<unary-op> ,op ,arg)
     (comp-unary-op op arg e))
    ((<if> ,b ,e1 ,e2)
     `(if ,(comp b e)
          ,(comp-block #f e1 e)
          ,(comp-block #f e2 e)))
    ((<compare> ,eleft ,ops ,rest)
     (let ((cops (til-list (map (lambda (x) (comp-op x)) ops)))
           (vals (til-list (cons (comp eleft e)
                                 (map (lambda (x) (comp x e)) rest)))))
       (@impl compare cops vals)))
    ((<call> ,fun ,args ,kws, ,stararg ,kwargs)
     (let ((c-fun (comp fun e))
           (c-args (map (lambda (x) (comp x e)) args)))
       `(call ,c-fun ,@c-args)))
    ((<num> ,n)
     `(const ,n))
    ((<name> ,name ,ctx)
     (let ((ret (lookup name e)))
       (if ret
           `(lexical ,name ,ret)
           (-> (toplevel name)))))
    ((<tuple> ,exps ,ctx)
     (comp-list-or-tuple exps e))
    ((<list> ,exps ,ctx)
     (comp-list-or-tuple exps e))
    (,any
     (debug "not matched:" any))))

(define (comp-list-or-tuple exps env)
  "Compiles a list or tuple expression into a list of values."
  (til-list (map (lambda (x) (car (comp x env))) exps)))

(define (comp-block toplevel stmts env)
  "Compiles a block of statements. Updates the environment in between
every statement."
  (define (get-ids targets)
    (pmatch targets
      (((<name> ,id <store>))
       (list id))))
  (let lp ((in stmts) (out '()))
    (pmatch in
      (((<assign> ,targets ,values) . ,rest)
       (guard (not toplevel))
       (let* ((argnames (get-ids targets))
              (gensyms  (map-gensym argnames))
              (args (comp values env))
              (stmt
               `(primcall call-with-values
                 ,(@impl assign-match-arguments `(const ,targets) args)
                 (lambda ()
                   (lambda-case
                    ((,argnames #f #f () () ,gensyms)
                     ,(comp-block #f rest
                                  (add2env env argnames gensyms))))))))
         (lp '() (cons stmt out))))
      ((,stmt . ,rest)
       (lp rest (cons (comp stmt env) out)))
      ('()
       (if (null? out)
           '(void)
           `(begin ,@(reverse! out)))))))

(define (add2env env args values)
  "Adds a list of symbols to the supplied environment."
  (append (pzip args values) env))

;; Handles all types of calls not involving kwargs and keyword
;; arguments.
(define (comp-fun-body id args body env)
  "Compiles a function body."
  (let* ((stararg (cadr args))
         (argnames (if (and (car args) stararg)
                       (append (map car (car args)) (list stararg))
                       (or (map car (car args)) stararg '())))
         (argconsts (map (lambda (x) `(const ,x)) argnames))
         (inits (map (lambda (x) (comp x env)) (seventh args)))
         (rest (gensym "rest$"))
         (argsym (gensym "args$"))
         ;; (kwargsym (gensym "kwargs$"))    kwargs not implemented yet
         (gensyms (map-gensym argnames)))
    `(lambda-case
      ((() #f ,rest
        (#f (#:args ,argsym ,argsym)) ;; (#:kwargs ,kwargsym ,kwargsym))
        ((const #f)) (,rest ,argsym))
       (let-values
         (primcall apply (primitive values)
                   ,(@impl fun-match-arguments
                           `(const ,id)
                           `(primcall list ,@argconsts)
                           `(const ,(if stararg #t #f))
                           (lex1 rest)
                           (lex1 argsym)
                           ;; (lex1 kwargsym)
                           `(primcall list ,@inits)))
         (lambda-case
          ((,argnames #f #f () () ,gensyms)
           ,(comp-block #f body (add2env env argnames gensyms)))))))))

(define (comp-op op)
  (define ops '((<gt> . >) (<lt> . <) (<gt-e> . >=) (<lt-e> . <=) (<eq> . equal?)))
  `(toplevel ,(lookup op ops)))

(define (comp-bin-op op e1 e2 env)
  (define ops '((<add> . +) (<sub> . -) (<mult> . *) (<div> . /)
                (<floor-div> . floor-quotient)
                (<mod> . euclidean-remainder)
                (<bit-xor> . logxor)
                (<bit-and> . logand)
                (<bit-or> . logior)))
  (let ((ce1 (comp e1 env))
        (ce2 (comp e2 env)))
    (pmatch op
      (<l-shift>
       `(call (toplevel ash) ,ce1 ,ce2))
      (<r-shift>
       `(call (toplevel ash) ,ce1 (call (toplevel -) ,ce2)))
      (,any
       `(call (toplevel ,(lookup op ops)) ,ce1 ,ce2)))))

(define (comp-bool-op op lst env)
  (define (and b a)
    `(if ,a ,b ,a))
  (define (or b a)
    `(if ,a ,a ,b))
  (let ((clst (map (lambda (x) (comp x env)) lst)))
    (pmatch op
      (<and>
       (reduce and (const #t) clst))
      (<or>
       (reduce or (const #f) clst)))))

(define (comp-unary-op op arg env)
  (pmatch op
    (<not>
     `(if ,(comp arg env) (const #f) (const #t)))))

;;;; The documentation for let-values in tree-il is incorrect. This is
;;;; an example for how it could be used.
;; (let-values (call (primitive apply)
;;                   (primitive values)
;;                   (call (primitive list) (const 3) (const 4)))
;;   (lambda-case (((a b) #f #f () () (a b)) (lexical b b))))

(define (lex1 sym)
  "A shorthand for lexical references where the symbol is the same as
the gensym."
  `(lexical ,sym ,sym))

(define (til-list a)
  "Makes a list of values into a tree-il list."
  `(primcall list ,@a))

(define (map-gensym argnames)
  (map (lambda (x) (gensym (string-append
                            (symbol->string x) "$")))
       argnames))

(define (test str)
  (let ((code ((@ (language python3 parse) read-python3) (open-input-string str))))
  (display-ln code)
  (compile-tree-il code '() '())))