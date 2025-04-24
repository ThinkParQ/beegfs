Support for BeeGFS <!-- omit in toc -->
==================

- [Overview](#overview)
- [Documentation](#documentation)
- [Community Support](#community-support)
- [Enterprise Support](#enterprise-support)
- [Support Lifecycle and Policies](#support-lifecycle-and-policies)
  - [BeeGFS Versioning](#beegfs-versioning)
  - [Release Lifecycle](#release-lifecycle)
  - [End-of-Life (EOL)](#end-of-life-eol)
  - [End-of-Support (EOS)](#end-of-support-eos)
  - [Example Scenarios](#example-scenarios)

# Overview
The BeeGFS community edition is available free of charge and shares a common codebase with our
[enterprise edition](https://www.beegfs.io/c/enterprise/enterprise-features/). The enterprise
edition includes support alongside additional features, and is how we fund ongoing BeeGFS
development. Regardless of whether you use the paid or community edition, we welcome [GitHub
issues](https://github.com/ThinkParQ/beegfs/issues) for bugs and feature requests from everyone in
our community.

**Please note:** GitHub issues are not the place for general questions on how to deploy or use
BeeGFS. Refer to the resources below. If you are unsure if your issue is a bug or "working as
designed," feel free to open an issue, and we will politely direct you to community support options
if necessary.

Thank you for being part of the BeeGFS community!

# Documentation

* [BeeGFS Documentation Site](https://doc.beegfs.io).
* [Building BeeGFS from sources](README.md) (for developers).
  * Check out the [contributing guide](CONTRIBUTING.md) if you'd like to contribute to BeeGFS!

# Community Support

BeeGFS is fortunate to have an active community of users who are often willing to help with general
questions on designing, deploying, and otherwise administering BeeGFS.

* [BeeGFS Google Groups](https://groups.google.com/g/fhgfs-user).
* [GitHub Discussions](https://github.com/ThinkParQ/beegfs/discussions).

While our team moderates these forums to ensure posts are on-topic and meet our [Code of
Conduct](code-of-conduct.md), we generally do not actively respond to posts. However, we are always
keeping an eye out for ways to improve BeeGFS and our documentation.

# Enterprise Support

The BeeGFS [enterprise edition](https://www.beegfs.io/c/enterprise/enterprise-features/) offers:

- Dedicated support from BeeGFS experts.
- Access to features not available in the community edition such as storage pools and quotas.
- Assistance with designing, deploying, and managing BeeGFS.

If you are a current BeeGFS enterprise customer with a valid support contract, please contact our
support team via the channels specified in your support contract documentation.

If you are interested in enterprise support, professional services, or learning more about the
additional features of the enterprise edition, please contact us at
[info@thinkparq.com](mailto:info@thinkparq.com).

# Support Lifecycle and Policies

## BeeGFS Versioning
Starting with BeeGFS 8, we will more strictly adhere to [Semantic Versioning](https://semver.org/)
to ensure a predictable and consistent support lifecycle. BeeGFS releases follow a MAJOR.MINOR.PATCH
versioning scheme, where:

* Major releases (e.g., 8.x) introduce breaking changes and new features.
* Minor releases (e.g., 8.1, 8.2) add functionality in a backward-compatible manner.
  * Note: Enabling new functionality typically requires all components to be on the same version
    otherwise older components may see errors about unknown functionality.
* Patch releases (e.g., 8.0.1, 8.1.2) contain only backward-compatible bug fixes and security
  updates.

## Release Lifecycle

* Latest Major Release (e.g., BeeGFS 8.x)
  * Actively maintained with bug fixes, security patches, and feature enhancements.
  * Receives support for new operating systems, kernels, and DOCA/OFED versions.
  * New deployments are recommended to use this version to ensure continued support without
    requiring a major upgrade.
* Previous Major Releases (e.g., BeeGFS 7.x)
  * Receive critical security and stability fixes until the release reaches EOL.
  * Receive best effort support for new operating systems, kernels, and DOCA/OFED versions until the
    release reaches EOL.
  * New deployments are strongly discouraged from using these versions.
* Minor and Patch Releases (e.g., 8.1.0, 8.0.1)
  * To receive bug fixes and security patches, users must upgrade to the latest available patch or
    minor version within their major release, as fixes will not be backported to older minor
    versions.

## End-of-Life (EOL)

* A BeeGFS release reaches EOL when it no longer receives regular updates, including:
  * Bug fixes
  * Security patches
  * Support for new OS/kernel/DOCA/OFED versions.
* New deployments should not be performed using a release that has reached EOL.
* Users with active support contracts for systems originally deployed on that version may still
  receive best-effort patches if they cannot upgrade to the latest major version due to
  incompatibilities.
  * Patches made after a release reaches EOL will typically be private unless they address a
    security vulnerability that affects all users.
* EOL timing: When a new major BeeGFS is made available, adoption trends, stability, and customer
  feedback are considered before announcing the EOL date for the previous major release. Generally
  the EOL date will be no less than 12 months from the release date of the new major version.

## End-of-Support (EOS)

* When a BeeGFS release reaches EOS it is considered obsolete and no further updates will be
  made—even for customers with a support contract.
* EOS timing: Depending on customer demand and extended support contracts EOS will typically follow
  EOL by 12-24 months.
* ThinkParQ support will assist all customers holding a valid support contract regardless if the
  BeeGFS version has reached EOS.

If support identifies a software issue in an EOS release, the recommended action will be to upgrade
to a supported version where the issue is (or can be) resolved. Support will assist as needed to
define a recommended upgrade path.

## Example Scenarios

* A user with/without support running BeeGFS 8.0.0 finds a bug before BeeGFS 7 reaches EOL:
  * If critical, it may be fixed in a patch release (8.0.1).
  * If less severe or found/fixed close to the next scheduled release, it may be fixed in a minor
    release (8.1.0).
  * If this bug also affects BeeGFS 7, it would be backported to the latest 7.x patch release (e.g.,
    7.4.x).
* A user with support running BeeGFS 7 after it reaches EOL finds a bug impacting performance or
  stability:
  * If the bug affects BeeGFS 8 and has not yet been fixed, a fix will be made in the latest 8.x
    release and the user advised to upgrade if possible.
  * If the user is unable to upgrade to BeeGFS 8 due to incompatibilities in their OS/kernel/etc.,
    the feasibility of patching BeeGFS 7 will be evaluated and decided on a case-by-case basis
    working in conjunction with the user to identify the best possible solution.
