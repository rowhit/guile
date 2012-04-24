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

(define-module (language python3 impl)
  #:use-module (language python3 commons)
  #:use-module (oop goops)
  #:use-module (srfi srfi-1)
  #:use-module (system base pmatch)
  #:export (compare fun-match-arguments make-python3-class setattr getattr))

(for-each
 (lambda (var)
   (if (defined? var) (module-remove! (current-module) var)))
 '(<py3-meta-class> <py3-instance>))

(define (fun-match-arguments id argnames has-stararg rest args inits)
  "`rest' represents all arguments passed to a method call. `args' is
the values passed to the `args' keyword argument. This method returns
the right arguments in the right order for use in a function body."
  (define (err-len)
    (error (string-concatenate `("Wrong number of arguments for "
                                 ,(symbol->string id)))))
  (let ((arg-len (if has-stararg
                     (1- (length argnames))
                     (length argnames))))
    (if args
        (set! rest (take-while (lambda (x) (not (equal? x #:args))) rest))
        (set! args '()))
    (let* ((first (append rest args))
           (first-len (length first))
           (inits+first-len (+ (length inits) first-len)))
      (if (< first-len arg-len)
          (if (< inits+first-len arg-len)
              (err-len)
              (append! first (drop inits (- inits+first-len arg-len)) '(())))
          (if (and (not has-stararg) (> first-len arg-len))
              (err-len)
              (call-with-values
                  (lambda () (split-at first arg-len))
                (lambda (a b) (if has-stararg `(,@a ,b) a))))))))

(define (compare os vs)
  (let lp ((ops os) (vals vs))
    (pmatch ops
      ((,op . '())
       (apply op vals))
      ((,op . ,rest)
       (if (op (car vals) (cadr vals))
           (lp (cdr ops) (cdr vals))
           #f))
      ((,a) (debug "not matched" ops)))))

;;; Python object implementation

(define-syntax-rule (make-attrs as)
  (let ((h (make-hash-table 7)))
    (map (lambda (pv)
           (let ((p (car pv)) (v (cdr pv)))
             (hashq-set! h p v)))
         as)
    h))

(define* (make-python3-class name bases body
                             #:key (keywords #f) (starargs #f)
                                   (kwargs #f)   (decos #f))
  "Creatas a Python 3 class called NAME. BASES is a list of base class
instances. BODY is an alist containing symbols mapped to values. If the
symbol used is #f just evaluate the value. Otherwise bind the value to
the class's standard `__dict__' attribute. This function returns a
callable python 3 class."
  ;; FIXME: bases is ignored atm.
  ;; TODO: find out what keywords etc does. Implement decos
  (let* ((sym (gensym (string-append (symbol->string name) "$")))
         (class-name (string->symbol (string-concatenate
                                      `("<" ,(symbol->string sym) ">")))))
    (eval
     `(begin
        (define-class ,class-name (<py3-object>)))
     (resolve-module '(language python3 impl)))
    (let ((class
           (make (@@ (language python3 impl) <py3-type>)
             #:d ((@@ (language python3 impl) make-attrs)
                  `((__bases__ . (,(@@ (language python3 impl) py3-object))))))))
      (slot-set! class 'procedure
                 (lambda ()
                   (let ((obj (make (module-ref (resolve-module
                                                 '(language python3 impl))
                                                class-name)
                                #:d (make-hash-table 7))))
                     (setattr obj '__bases__ (list class))
                     (slot-set! obj 'procedure
                                (lambda (. rest)
                                  (apply (getattr class '__call__) rest)))
                     obj)))
      (map (lambda (x)
             (if (car x)
                 (setattr class (car x) (cdr x))
                 (cdr x)))
           body)
      class)))

(define *undefined* ((@@ (oop goops) make-unbound)))

(define-class <py3-meta-class> (<class>))

(define-method (slot-missing (class <py3-meta-class>) instance slot-name)
  (getattr instance slot-name))

(define-method (slot-missing (class <py3-meta-class>) instance slot-name value)
  (setattr instance slot-name value)
  (values))

(define-class <py3-object> (<applicable-struct>)
  (id #:getter py-id #:init-form (gensym "pyclass$"))
  (type #:getter py-type #:init-keyword #:t)
  (dict #:getter py-dict #:init-keyword #:d)
  #:metaclass <py3-meta-class>)

(define py3-object
  (make <py3-object>
    #:d (make-attrs '((__bases__ . ())))))

(define-class <py3-type> (<py3-object>))

(define-method (getattr (o <py3-object>) (p <string>))
  (getattr o (string->symbol p)))

(define-method (getattr (o <py3-object>) p)
  (let ((ret (lookupattr o p)))
    (if ret
        (cdr ret)
        (error (string-concatenate
                `(,(object->string o) " has no attribute "
                  ,(symbol->string p)))))))

(define-method (lookupattr (o <py3-object>) p)
  (let ((h (hashq-get-handle (py-dict o) p)))
    (if h
        h
        (let lp ((classes (getattr o '__bases__)))
          (pmatch classes
            (()
             #f)
            ((,c . ,rest)
             (or (lookupattr c p) (lp rest))))))))

(define-method (setattr (o <py3-object>) (p <string>) v)
  (setattr o (string->symbol p) v))

(define-method (setattr (o <py3-object>) p v)
  (hashq-set! (py-dict o) p v))

(define-method (setattr (o <py3-object>) (ps <list>))
  (map (lambda (pv)
         (let ((p (car pv)) (v (cdr pv)))
           (setattr o p v)))
       ps))

;; NB: We ignore __base__ for now, see:
;; http://code.activestate.com/lists/python-list/334282/

;;; Base classes

(define class-type
  (make <py3-object>
    #:d (make-attrs '((__bases__ . (object))
                      (__doc__ . "type(object) -> the object's type\ntype(name, bases, dict) -> a new type")
                      (__name__ . "type")))))

(define object-type
  (make <py3-object>
    #:d (make-attrs '((__bases__ . ())
                      (__doc__ . "The most base object")
                      (__name__ . "object")))))

;;; Type classes

(define not-implemented-type
  (make <py3-object>
    #:d (make-attrs '((__bases__ . (type))
                      (__name__ . "NotImplementedType")))))

(define none-type
  (make <py3-object>
    #:d (make-attrs '((__bases__ . (type))
                      (__name__ . "NoneType")))))

(define ellipsis-type
  (make <py3-object>
    #:d (make-attrs '((__bases__ . (type))
                      (__name__ . "Ellipsis")))))

(define tuple-type
  (make <py3-object>
    #:d (make-attrs '((__bases__ . (type))
                      (__name__ . "tuple")))))

(define str-type
  (make <py3-object>
    #:d (make-attrs '((__bases__ . (type))
                      (__name__ . "str")))))

(define list-type
  (make <py3-object>
    #:d (make-attrs '((__bases__ . (type))
                      (__name__ . "list")))))

;; >>> object.__class__
;; <class 'type'>
;; >>> dir(object.__class__)
;; '__abstractmethods__'  -> Unkown, not necessary to implement
;; '__bases__'            ->
;; '__call__'             -> Is called if the object is called as a method
;; '__class__'            -> Pointer to the class object
;; '__setattr__'          ->
;; '__delattr__'          -> Called when deleting a attribute using `delete foo.bar`
;; '__dict__'             -> A dict containing all the attributes of the object
;; '__dictoffset__'       ->
;; '__doc__'              -> A function/objects documentation string or None if not set
;; '__eq__'               -> Called by the == operator
;; '__ge__'               -> Called by the <= operator
;; '__gt__'               -> Called by the < operator
;; '__le__'               -> Called by the >= operator
;; '__lt__'               -> Called by the > operator
;; '__ne__'               -> Called by the != operator, calls __eq__ if not defined
;; '__flags__'            ->
;; '__format__'           -> Object defined function that returns a string for use by the format() function, takes two arguments __format__(self, format_spec)
;; '__getattribute__'     -> Almost always called when accessing attributes on an object, will get called instead of __getattr__ if this is defined.
;; '__hash__'             ->
;; '__init__'             -> Called on initialization of an object instance.
;; '__instancecheck__'    -> takes two arguments (self, instance) and returns true if instance is a direct or indirect instance of the objects class.
;; '__itemsize__'         -> How big the items in a collection is
;; '__basicsize__'        -> Fixed size independant on number of objects
;; '__module__'           -> The name of the module the function is defined in or None if not available.
;; '__mro__'              ->
;; '__name__'             ->
;; '__new__'              ->
;; '__prepare__'          ->
;; '__reduce__'           ->
;; '__reduce_ex__'        ->
;; '__repr__'             ->
;; '__sizeof__'           ->
;; '__str__'              ->
;; '__subclasscheck__'    ->
;; '__subclasses__'       ->
;; '__subclasshook__'     ->
;; '__weakrefoffset__'    ->
;; 'mro']                 ->

;; >>> object().__class__
;; <class 'object'>
;; >>> dir(object().__class__)
;; ['__class__'
;; '__delattr__'
;; '__doc__'
;; '__eq__'
;; '__format__'
;; '__ge__'
;; '__getattribute__'
;; '__gt__'
;; '__hash__'
;; '__init__'
;; '__le__'
;; '__lt__'
;; '__ne__'
;; '__new__'
;; '__reduce__'
;; '__reduce_ex__'
;; '__repr__'
;; '__setattr__'
;; '__sizeof__'
;; '__str__'
;; '__subclasshook__']
