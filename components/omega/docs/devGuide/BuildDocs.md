(omega-dev-building-docs)=

# Building the Documentation

As long as you have followed the procedure in {ref}`omega-dev-conda-env` for
setting up your conda environment, you will already have the packages available
that you need to build the documentation.

From the root of an OMEGA branch, run the following script to build the docs:

```bash
cd components/omega/docs
make clean
make html
```

You can view the documentation by opening `_build/html/index.html` in your
choice of browser.

If you run into errors or warnings related to changes you have meade when you
build the docs, check with the development team for help on fixing them.
