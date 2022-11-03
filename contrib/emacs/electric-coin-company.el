;;; electric-coin-company.el --- ECC-specific Emacs configuration  -*- lexical-binding: t; -*-

;;; Commentary:

;; This file is loaded as part of .dir-locals.el in this project. You shouldn’t
;; have to do anything to get ECC-preferred formatting in this project.

;; Do not put anything in here that would affect a user’s experience globally.
;; Use “../../.dir-locals.el” for any variables, so that they stay local to the
;; project. This module contains things that are either used by .dir-locals or
;; that a user can choose to enable.

;; Codifies the C & C++ coding style preferred at ECC.
;;
;; TODO: We may actually want to define separate styles, including one for
;;       Bitcoin style, so you can match the style of the file you’re editing.
(c-add-style "electric-coin-company"
  '("k&r" ; based on K & R style, with the following changes
     ;; indent amount
     (c-basic-offset . 4)
     (c-offsets-alist
       ;; parameter & argument lists should use double the standard indent,
       ;; rather than aligning with the open paren
       (arglist-intro . ++)
       ;; `case` should use standard indent, rather than aligning with `switch`
       (case-label . +)
       (inline-open . 0))))

(provide 'electric-coin-company)
