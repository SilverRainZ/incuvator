
(in-package :hurd-example-translators)

(assert (= (length ext:*args*) 1))
(defconstant +target-link+ (first ext:*args*))

(defclass link-translator (translator)
  ((name :initform "link-translator"
         :documentation "Translator name"))
  (:documentation "The link-translator."))

(define-callback make-root-node link-translator
                 (underlying-stat)
  (let ((obj (make-instance 'node
                            :stat (make-stat underlying-stat
                                             :type :lnk))))
    (setf (link obj) +target-link+)
    obj))

(define-callback report-access link-translator
                 (node user)
  (when (has-access-p node user 'read)
    '(:read)))

(defun main ()
  (run-translator (make-instance 'link-translator)))

(main)

;; TODO:
;; echo /mydir/a/b > foo = linking now to /mydir/a/b
