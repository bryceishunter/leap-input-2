Releasing
=========

For maintainers of Leapdesk KVM.

1. Collect the release notes, then commit them:

       towncrier build --version X.Y.Z --date `date -u +%F`

   `towncrier` skips fragment files whose names it doesn't recognise, so check
   `doc/newsfragments` for leftovers.

2. Update the version in `cmake/Version.cmake` and `dist/debian/changelog`.

3. Tag and push:

       git tag -s vX.Y.Z -m vX.Y.Z
       git push origin master vX.Y.Z

4. Draft the release at
   https://github.com/bryceishunter/leapdesk-kvm/releases, using the tag as the
   title and the collected notes as the description, and attach the build
   artifacts.
